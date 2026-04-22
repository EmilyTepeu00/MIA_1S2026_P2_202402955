// BACKEND CON API REST

#include <iostream>
#include <string>
#include <fstream>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <vector>

// Libreria Crow para API REST
#include "crow.h"

using namespace std;


// ******** ESTRUCTURAS DE DATOS ********

// ESTRUCTURA PARTITION
struct Partition {
    char part_status;   // "0" no montado y "1" montado
    char part_type;     // "P" primario, "E" extendido
    char part_fit;      // "B" best fit, "F" first fit y "W" = worst fit
    int part_start;
    int part_size;
    char part_name[16];
    int part_correlative;   // -1 si es no montado
    char part_id[4];
    
    // Constructor
    Partition() {
        part_status = '0'; part_type = 'P'; part_fit = 'W'; // Worst fit por default
        part_start = -1; part_size = 0;
        memset(part_name, 0, 16);
        part_correlative = -1;
        memset(part_id, 0, 4);
    }
};

// ESTRUCTURA MBR
struct MBR {
    int mbr_size;
    time_t mbr_creation_date;
    int mbr_dsk_signature;           // Numero random del disco
    Partition mbr_partitions[4];
    
    // Constructor
    MBR() {
        mbr_size = 0;
        mbr_creation_date = time(nullptr);  // Fecha actual
        mbr_dsk_signature = rand() % 10000;
    }
};

// ESTRUCTURA EBR
struct EBR {
    char part_mount;        // '0' desmontado, '1' montado
    char part_fit;          // 'B', 'F', 'W'
    int part_start;         // Byte donde inicia la particion logica
    int part_size;
    int part_next;          // Byte donde esta el siguiente EBR (-1 si no hay)
    char part_name[16];

    // Constructor
    EBR() {
        part_mount = '0';
        part_fit = 'W';
        part_start = -1;
        part_size = 0;
        part_next = -1;
        memset(part_name, 0, sizeof(part_name));
    }
};

// **** ESTRUCTURAS PARA EXT2 ****

// ESTRUCTURA PARA INODOS
struct Inodo {
    int i_uid;                  // UID del propietario
    int i_gid;                  // GID del grupo
    int i_size;                 // Tamaño del archivo
    time_t i_atime;             // Ultimo acceso
    time_t i_ctime;             // Creacion
    time_t i_mtime;             // Ultima modificacion
    int i_block[15];            // Apuntadores directos e indirectos
    char i_type;                // 1 = archivo, 0 = carpeta
    char i_perm[3];             // Permisos UGO (rwx)

    Inodo() {
        i_uid = 0;
        i_gid = 0;
        i_size = 0;
        i_atime = time(nullptr);
        i_ctime = time(nullptr);
        i_mtime = time(nullptr);
        for (int i = 0; i < 15; i++) i_block[i] = -1;
        i_type = 0;
        i_perm[0] = '7';  // rwx por default para root
        i_perm[1] = '7';
        i_perm[2] = '7';
    }
};

// ESTRUCTURA PARA SUPER BLOQUE
struct Superblock {
    int s_filesystem_type;       // 2 para EXT2
    int s_inodes_count;          // Total de inodos
    int s_blocks_count;          // Total de bloques
    int s_free_blocks_count;     // Bloques libres
    int s_free_inodes_count;     // Inodos libres
    time_t s_mtime;              // Ultimo montaje
    time_t s_umtime;             // Ultimo desmontaje
    int s_mnt_count;             // Veces montado
    int s_magic;                 // 0xEF53
    int s_inode_size;            // Tamaño del inodo
    int s_block_size;            // Tamaño del bloque (64 bytes)
    int s_first_inode;           // Primer inodo libre
    int s_first_block;           // Primer bloque libre
    int s_bm_inode_start;        // Inicio bitmap de inodos
    int s_bm_block_start;        // Inicio bitmap de bloques
    int s_inode_start;           // Inicio tabla de inodos
    int s_block_start;           // Inicio tabla de bloques

    Superblock() {
        s_filesystem_type = 2;
        s_inodes_count = 0;
        s_blocks_count = 0;
        s_free_blocks_count = 0;
        s_free_inodes_count = 0;
        s_mtime = time(nullptr);
        s_umtime = time(nullptr);
        s_mnt_count = 0;
        s_magic = 0xEF53;
        s_inode_size = sizeof(Inodo);
        s_block_size = 64;
        s_first_inode = 0;
        s_first_block = 0;
        s_bm_inode_start = 0;
        s_bm_block_start = 0;
        s_inode_start = 0;
        s_block_start = 0;
    }
};

// ESTRUCTURA EL CONTENIDO DEL JOURNAL
struct Information {
    char i_operation[10];   // Operacion realizada ("CREATE", "DELETE")
    char i_path[32];        // Ruta donde se realizo
    char i_content[64];     // Contenido (si es archivo)
    float i_date;           // Fecha y hora (time_t)
    
    Information() {
        memset(i_operation, 0, 10);
        memset(i_path, 0, 32);
        memset(i_content, 0, 64);
        i_date = time(nullptr);
    }
};

// ESTRUCTURA PARA JOURNAL
struct Journal {
    int j_count;            // Conteo de entradas
    Information j_content;  // Contenido del journal
    
    Journal() {
        j_count = 0;
    }
};

// ESTRUCTURA PARA BLOQUES DE CARPETAS
struct BloqueCarpeta {
    struct Content {
        char b_name[12];   // Nombre del archivo/carpeta
        int b_inodo;       // Inodo asociado
    } b_content[4];        // 4 entradas por bloque (64 bytes)
};

// ESTRUCTURA PARA BLOQUES DE ARCHIVOS
struct BloqueArchivo {
    char b_content[64];
};

// ESTRUCTURA APRA BLOQUES DE APUNTADORES
struct BloqueApuntadores {
    int b_pointers[16];
};


// ESTRUCTURAS PARA MONTAJE (EN MEMORIA)
struct Montada {
    string path_disco;
    string nombre_particion;
    string id;
    int part_start;
    int part_size;
    char part_type;
    
    Montada() {
        path_disco = "";
        nombre_particion = "";
        id = "";
        part_start = -1;
        part_size = 0;
        part_type = ' ';
    }
};

// VARIABLES GLOBALES
vector<Montada> particiones_montadas;
int contador_por_letra[26] = {0};


// ******** FUNCIONES UTILITARIAS ********

// --- FUNCION: leerMBR ---
bool leerMBR(string path, MBR &mbr) {
    ifstream archivo(path, ios::binary);
    if (!archivo.is_open()) return false;
    
    archivo.seekg(0);   // Seek Get: mueve el puntero de lectura al inicio
    archivo.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
    archivo.close();
    return true;
}

// --- FUNCION: escribirMBR ---
bool escribirMBR(string path, MBR &mbr) {
    ofstream archivo(path, ios::binary | ios::in | ios::out);
    if (!archivo.is_open()) return false;
    
    archivo.seekp(0);   // Seek Put: mueve el puntero de escritura al inicio
    archivo.write(reinterpret_cast<const char*>(&mbr), sizeof(MBR));
    archivo.close();
    return true;
}

// --- FUNCION: calcularEspaciosLibres ---
// Para devolver la lista de espacios libres entre particiones (inicio, tamaño)
vector<pair<int, int>> calcularEspaciosLibres(MBR &mbr, int tamano_disco) {
    vector<pair<int, int>> espacios;
    
    // Ordenar particiones activas (part_start)
    vector<Partition> particiones_activas;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            particiones_activas.push_back(mbr.mbr_partitions[i]);
        }
    }
    
    // Ordenar por burbuja simple (part_start)
    for (int i = 0; i < particiones_activas.size(); i++) {
        for (int j = i + 1; j < particiones_activas.size(); j++) {
            if (particiones_activas[i].part_start > particiones_activas[j].part_start) {
                swap(particiones_activas[i], particiones_activas[j]);
            }
        }
    }
    
    // Espacio antes de la primera particion
    int inicio = sizeof(MBR);  // Despues del MBR
    if (particiones_activas.empty()) {
        espacios.push_back({inicio, tamano_disco - inicio});
    } else {
        // Entre MBR y primera particion
        if (inicio < particiones_activas[0].part_start) {
            espacios.push_back({inicio, particiones_activas[0].part_start - inicio});
        }
        
        // Entre particiones
        for (int i = 0; i < particiones_activas.size() - 1; i++) {
            int fin_actual = particiones_activas[i].part_start + particiones_activas[i].part_size;
            int inicio_siguiente = particiones_activas[i + 1].part_start;
            if (fin_actual < inicio_siguiente) {
                espacios.push_back({fin_actual, inicio_siguiente - fin_actual});
            }
        }
        
        // Despues de la ultima particion
        int ultimo_fin = particiones_activas.back().part_start + particiones_activas.back().part_size;
        if (ultimo_fin < tamano_disco) {
            espacios.push_back({ultimo_fin, tamano_disco - ultimo_fin});
        }
    }
    
    return espacios;
}

// --- FUNCION: elegirAJuste ---
int elegirAjuste(vector<pair<int, int>> &espacios, int tamano, char fit) {
    if (espacios.empty()) return -1;
    
    if (fit == 'F') {  // FIRST FIT
        for (int i = 0; i < espacios.size(); i++) {
            if (espacios[i].second >= tamano) return i;
        }
    }
    else if (fit == 'B') {  // BEST FIT
        int mejor_idx = -1;
        int mejor_tamano = INT_MAX;
        for (int i = 0; i < espacios.size(); i++) {
            if (espacios[i].second >= tamano && espacios[i].second < mejor_tamano) {
                mejor_tamano = espacios[i].second;
                mejor_idx = i;
            }
        }
        return mejor_idx;
    }
    else if (fit == 'W') {  // WORST FIT
        int peor_idx = -1;
        int peor_tamano = -1;
        for (int i = 0; i < espacios.size(); i++) {
            if (espacios[i].second >= tamano && espacios[i].second > peor_tamano) {
                peor_tamano = espacios[i].second;
                peor_idx = i;
            }
        }
        return peor_idx;
    }
    return -1;
}

// --- FUNCION: leerEBR ---
bool leerEBR(string path, int pos, EBR &ebr) {
    ifstream archivo(path, ios::binary);
    if (!archivo.is_open()) return false;

    archivo.seekg(pos);
    archivo.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
    archivo.close();
    return true;
}

// --- FUNCION: escribirEBR ---
bool escribirEBR(string path, int pos, EBR &ebr) {
    ofstream archivo(path, ios::binary | ios::in | ios::out);
    if (!archivo.is_open()) return false;

    archivo.seekp(pos);
    archivo.write(reinterpret_cast<const char*>(&ebr), sizeof(EBR));
    archivo.close();
    return true;
}

// --- FUNCION: crearEBRInicial ---
bool crearEBRInicial(string path, int start, int size, string fit, string name) {
    EBR nuevo_ebr;
    nuevo_ebr.part_mount = '0';
    nuevo_ebr.part_fit = fit[0];
    nuevo_ebr.part_start = start + sizeof(EBR);  // Datos despues del EBR
    nuevo_ebr.part_size = 0;  // 0 -> vacio
    nuevo_ebr.part_next = -1;
    strcpy(nuevo_ebr.part_name, name.c_str());

    // Escribir EBR al inicio de la particion extendida
    ofstream archivo(path, ios::binary | ios::in | ios::out);
    if (!archivo.is_open()) return false;

    archivo.seekp(start);
    archivo.write(reinterpret_cast<const char*>(&nuevo_ebr), sizeof(EBR));
    archivo.close();
    return true;
}

// --- FUNCION: generarID ---
string generarID(string path_disco) {
    // Contar cuántas particiones de este disco ya están montadas
    int contador = 0;
    char letra = 'A';
    
    for (const auto& m : particiones_montadas) {
        if (m.path_disco == path_disco) {
            contador++;
            // La letra será la del último montaje
            if (m.id.length() >= 4) {
                letra = m.id[3];
            }
        }
    }
    
    // Si no hay montajes previos, empezar con A y contador 1
    if (contador == 0) {
        return "551A";
    }
    
    // Si ya hay montajes, aumentar contador
    int nuevo_num = contador + 1;
    string num_str;
    if (nuevo_num < 10) {
        num_str = "0" + to_string(nuevo_num);
    } else {
        num_str = to_string(nuevo_num);
    }
    
    return "55" + num_str + letra;
}

// ---FUNCION: calcularN ---
int calcularN(int tamano_particion) {
    // Formula EXT3: tamano_particion = sizeof(Superblock) + n * sizeof(Journal) + n + 3n + n*sizeof(Inodo) + 3n*64
    int n = 0;
    int superblock_size = sizeof(Superblock);
    int journal_size = sizeof(Journal);
    
    while (true) {
        int necesario = superblock_size + (n * journal_size) + n + (3 * n) + (n * sizeof(Inodo)) + (3 * n * 64);
        if (necesario <= tamano_particion) {
            n++;
        } else {
            break;
        }
    }
    return n - 1;  // Ultimo valor que cabe
}


// ******** FUNCIONES PARA BITMAPS ********

// --- FUNCION: marcarBitInodo ---
// Marca un inodo como ocupado (1) en el bitmap
bool marcarBitInodo(string path, int start_bm, int n, int pos) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_bm + pos);
    char bit = 1;
    disco.write(&bit, 1);
    disco.close();
    return true;
}

// --- FUNCION: liberarBitInodo ---
// Marca un inodo como libre (0) en el bitmap
bool liberarBitInodo(string path, int start_bm, int n, int pos) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_bm + pos);
    char bit = 0;
    disco.write(&bit, 1);
    disco.close();
    return true;
}

// --- FUNCION: marcarBitBloque ---
// Marca un bloque como ocupado
bool marcarBitBloque(string path, int start_bm, int n, int pos) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_bm + pos);
    char bit = 1;
    disco.write(&bit, 1);
    disco.close();
    return true;
}

// --- FUNCION: liberarBitBloque ---
// Marca un bloque como libre
bool liberarBitBloque(string path, int start_bm, int n, int pos) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_bm + pos);
    char bit = 0;
    disco.write(&bit, 1);
    disco.close();
    return true;
}


// ******** FUNCIONES PARA INODOS ********

// --- FUNCION: leerInodo (tabla) ---
bool leerInodo(string path, int start_inodos, int pos, Inodo &inodo) {
    ifstream disco(path, ios::binary);
    if (!disco.is_open()) return false;
    
    disco.seekg(start_inodos + pos * sizeof(Inodo));
    disco.read(reinterpret_cast<char*>(&inodo), sizeof(Inodo));
    disco.close();
    return true;
}

// --- FUNCION: escribirInodo (tabla) ---
bool escribirInodo(string path, int start_inodos, int pos, Inodo &inodo) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_inodos + pos * sizeof(Inodo));
    disco.write(reinterpret_cast<const char*>(&inodo), sizeof(Inodo));
    disco.close();
    return true;
}


// ******** FUNCIONES PARA BLOQUES ********

// --- FUNCION: leerBloqueCarpeta ---
bool leerBloqueCarpeta(string path, int start_bloques, int pos, BloqueCarpeta &bloque) {
    ifstream disco(path, ios::binary);
    if (!disco.is_open()) return false;
    
    disco.seekg(start_bloques + pos * 64);
    disco.read(reinterpret_cast<char*>(&bloque), 64);
    disco.close();
    return true;
}

// --- FUNCION: escribirBloqueCarpeta ---
bool escribirBloqueCarpeta(string path, int start_bloques, int pos, BloqueCarpeta &bloque) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_bloques + pos * 64);
    disco.write(reinterpret_cast<const char*>(&bloque), 64);
    disco.close();
    return true;
}

// --- FUNCION: leerBloqueArchivo ---
bool leerBloqueArchivo(string path, int start_bloques, int pos, BloqueArchivo &bloque) {
    ifstream disco(path, ios::binary);
    if (!disco.is_open()) return false;
    
    disco.seekg(start_bloques + pos * 64);
    disco.read(reinterpret_cast<char*>(&bloque), 64);
    disco.close();
    return true;
}

// --- FUNCION: escribirBloqueArchivo ---
bool escribirBloqueArchivo(string path, int start_bloques, int pos, BloqueArchivo &bloque) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_bloques + pos * 64);
    disco.write(reinterpret_cast<const char*>(&bloque), 64);
    disco.close();
    return true;
}

// --- FUNCION: leerBloqueApuntadores ---
bool leerBloqueApuntadores(string path, int start_bloques, int pos, BloqueApuntadores &bloque) {
    ifstream disco(path, ios::binary);
    if (!disco.is_open()) return false;
    
    disco.seekg(start_bloques + pos * 64);
    disco.read(reinterpret_cast<char*>(&bloque), 64);
    disco.close();
    return true;
}

// --- FUNCION: escribirBloqueApuntadores ---
bool escribirBloqueApuntadores(string path, int start_bloques, int pos, BloqueApuntadores &bloque) {
    fstream disco(path, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return false;
    
    disco.seekp(start_bloques + pos * 64);
    disco.write(reinterpret_cast<const char*>(&bloque), 64);
    disco.close();
    return true;
}


// ******** FUNCIONES AUXILIARES ********

// --- FUNCION: obtenerRutaArray ---
vector<string> obtenerRutaArray(string ruta) {
    vector<string> partes;
    stringstream ss(ruta);
    string parte;
    
    while (getline(ss, parte, '/')) {
        if (!parte.empty()) {
            partes.push_back(parte);
        }
    }
    return partes;
}

// --- FUNCION: buscarInodoPorRuta ---
int buscarInodoPorRuta(string path_disco, int start_inodos, int start_bloques, string ruta) {
    if (ruta == "/") return 0;  // Inodo raiz
    
    vector<string> partes = obtenerRutaArray(ruta);
    int inodo_actual = 0;  // Raiz
    
    for (const string& nombre : partes) {
        // Leer inodo actual
        Inodo inodo;
        if (!leerInodo(path_disco, start_inodos, inodo_actual, inodo)) {
            return -1;
        }
        
        if (inodo.i_type != 0) {  // No es carpeta
            return -1;
        }
        
        // Buscar en los bloques de la carpeta
        bool encontrado = false;
        for (int i = 0; i < 15 && inodo.i_block[i] != -1; i++) {
            BloqueCarpeta bloque;
            if (!leerBloqueCarpeta(path_disco, start_bloques, inodo.i_block[i], bloque)) {
                continue;
            }
            
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo != -1) {
                    string nombre_actual(bloque.b_content[j].b_name);
                    if (nombre_actual == nombre) {
                        inodo_actual = bloque.b_content[j].b_inodo;
                        encontrado = true;
                        break;
                    }
                }
            }
            if (encontrado) break;
        }
        
        if (!encontrado) return -1;
    }
    
    return inodo_actual;
}

// --- FUNCION: obtenerInodoLibre ---
int obtenerInodoLibre(string path_disco, int start_bm, int n) {
    ifstream disco(path_disco, ios::binary);
    if (!disco.is_open()) return -1;
    
    disco.seekg(start_bm);
    char* bitmap = new char[n];
    disco.read(bitmap, n);
    disco.close();
    
    for (int i = 0; i < n; i++) {
        if (bitmap[i] == 0) {
            delete[] bitmap;
            return i;
        }
    }
    
    delete[] bitmap;
    return -1;
}

// --- FUNCION: obtenerBloqueLibre ---
int obtenerBloqueLibre(string path_disco, int start_bm, int n) {
    ifstream disco(path_disco, ios::binary);
    if (!disco.is_open()) return -1;
    
    disco.seekg(start_bm);
    char* bitmap = new char[n];
    disco.read(bitmap, n);
    disco.close();
    
    for (int i = 0; i < n; i++) {
        if (bitmap[i] == 0) {
            delete[] bitmap;
            return i;
        }
    }
    
    delete[] bitmap;
    return -1;
}

// --- FUNCION: obtenerPadreYNombre ---
pair<string, string> obtenerPadreYNombre(string ruta) {
    size_t ultimo_slash = ruta.find_last_of('/');
    if (ultimo_slash == string::npos) {
        return {"/", ruta};  // Ruta relativa, se asume raiz
    }
    string padre = ruta.substr(0, ultimo_slash);
    string nombre = ruta.substr(ultimo_slash + 1);
    if (padre.empty()) padre = "/";
    return {padre, nombre};
}

// --- FUNCION: mkdirInterno (crea una sola carpeta) ---
string mkdirInterno(string path_disco, Superblock &sb, string path, int uid, int gid) {
    // Separar ruta en padre y nombre
    auto [ruta_padre, nombre_carpeta] = obtenerPadreYNombre(path);
    
    // Verificar que existe la carpeta padre
    int inodo_padre = buscarInodoPorRuta(path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre);
    if (inodo_padre == -1) {
        return "ERROR: La carpeta padre no existe";
    }
    
    // Verificar que no exista ya
    int inodo_existente = buscarInodoPorRuta(path_disco, sb.s_inode_start, sb.s_block_start, path);
    if (inodo_existente != -1) {
        return "ERROR: La carpeta ya existe";
    }
    
    // Buscar inodo libre
    int nuevo_inodo_pos = obtenerInodoLibre(path_disco, sb.s_bm_inode_start, sb.s_inodes_count);
    if (nuevo_inodo_pos == -1) {
        return "ERROR: No hay inodos libres";
    }
    
    // Buscar bloque libre para la carpeta
    int nuevo_bloque_pos = obtenerBloqueLibre(path_disco, sb.s_bm_block_start, sb.s_blocks_count);
    if (nuevo_bloque_pos == -1) {
        return "ERROR: No hay bloques libres";
    }
    
    // Crear nuevo inodo de carpeta
    Inodo nuevo_inodo;
    nuevo_inodo.i_uid = uid;
    nuevo_inodo.i_gid = gid;
    nuevo_inodo.i_size = 0;
    nuevo_inodo.i_atime = time(nullptr);
    nuevo_inodo.i_ctime = time(nullptr);
    nuevo_inodo.i_mtime = time(nullptr);
    nuevo_inodo.i_type = 0;       // Carpeta
    nuevo_inodo.i_perm[0] = '6';  // rw-
    nuevo_inodo.i_perm[1] = '6';  // rw-
    nuevo_inodo.i_perm[2] = '4';  // r--
    nuevo_inodo.i_block[0] = nuevo_bloque_pos;  // Apuntar al primer bloque
    
    // Crear bloque de carpeta (con . y ..)
    BloqueCarpeta bloque;
    memset(&bloque, 0, sizeof(BloqueCarpeta));
    
    // Entrada "." (esta carpeta)
    strcpy(bloque.b_content[0].b_name, ".");
    bloque.b_content[0].b_inodo = nuevo_inodo_pos;
    
    // Entrada ".." (carpeta padre)
    strcpy(bloque.b_content[1].b_name, "..");
    bloque.b_content[1].b_inodo = inodo_padre;
    
    // Las demas entradas vacias
    for (int i = 2; i < 4; i++) {
        bloque.b_content[i].b_inodo = -1;
    }
    
    // Escribir bloque de carpeta
    if (!escribirBloqueCarpeta(path_disco, sb.s_block_start, nuevo_bloque_pos, bloque)) {
        return "ERROR: No se pudo escribir bloque de carpeta";
    }
    
    // Marcar bloque como ocupado
    marcarBitBloque(path_disco, sb.s_bm_block_start, sb.s_blocks_count, nuevo_bloque_pos);
    
    // Escribir inodo
    marcarBitInodo(path_disco, sb.s_bm_inode_start, sb.s_inodes_count, nuevo_inodo_pos);
    if (!escribirInodo(path_disco, sb.s_inode_start, nuevo_inodo_pos, nuevo_inodo)) {
        return "ERROR: No se pudo escribir inodo";
    }
    
    // Agregar entrada en carpeta padre
    Inodo inodo_padre_obj;
    if (!leerInodo(path_disco, sb.s_inode_start, inodo_padre, inodo_padre_obj)) {
        return "ERROR: No se pudo leer inodo padre";
    }
    
    // Buscar espacio en bloques de la carpeta padre
    bool entrada_agregada = false;
    for (int i = 0; i < 12 && inodo_padre_obj.i_block[i] != -1; i++) {
        BloqueCarpeta bloque_padre;
        if (!leerBloqueCarpeta(path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque_padre)) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (bloque_padre.b_content[j].b_inodo == -1) {
                strcpy(bloque_padre.b_content[j].b_name, nombre_carpeta.c_str());
                bloque_padre.b_content[j].b_inodo = nuevo_inodo_pos;
                
                if (!escribirBloqueCarpeta(path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque_padre)) {
                    return "ERROR: No se pudo actualizar bloque padre";
                }
                entrada_agregada = true;
                break;
            }
        }
        if (entrada_agregada) break;
    }
    
    if (!entrada_agregada) {
        return "ERROR: No hay espacio en carpeta padre para nueva entrada";
    }
    
    return "OK";
}

// --- FUNCION: mkfileInterno (crea archivo con contenido) ---
string mkfileInterno(string path_disco, Superblock &sb, string path, string contenido, int uid, int gid) {
    auto [ruta_padre, nombre_archivo] = obtenerPadreYNombre(path);
    
    int inodo_padre = buscarInodoPorRuta(path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre);
    if (inodo_padre == -1) return "ERROR: Carpeta padre no existe";
    
    int nuevo_inodo_pos = obtenerInodoLibre(path_disco, sb.s_bm_inode_start, sb.s_inodes_count);
    if (nuevo_inodo_pos == -1) return "ERROR: No hay inodos libres";
    
    Inodo nuevo_inodo;
    nuevo_inodo.i_uid = uid;
    nuevo_inodo.i_gid = gid;
    nuevo_inodo.i_size = contenido.length();
    nuevo_inodo.i_type = 1;
    nuevo_inodo.i_perm[0] = '6';
    nuevo_inodo.i_perm[1] = '6';
    nuevo_inodo.i_perm[2] = '4';
    
    // Asignar bloques para el contenido
    int pos = 0;
    int bloque_idx = 0;
    int tam = contenido.length();
    
    while (tam > 0 && bloque_idx < 12) {
        int bloque_libre = obtenerBloqueLibre(path_disco, sb.s_bm_block_start, sb.s_blocks_count);
        if (bloque_libre == -1) return "ERROR: No hay bloques libres";
        
        nuevo_inodo.i_block[bloque_idx] = bloque_libre;
        marcarBitBloque(path_disco, sb.s_bm_block_start, sb.s_blocks_count, bloque_libre);
        
        BloqueArchivo bloque;
        memset(bloque.b_content, 0, 64);
        int copiar = min(64, tam);
        strncpy(bloque.b_content, contenido.substr(pos, copiar).c_str(), copiar);
        
        if (!escribirBloqueArchivo(path_disco, sb.s_block_start, bloque_libre, bloque)) {
            return "ERROR: No se pudo escribir bloque";
        }
        
        tam -= copiar;
        pos += copiar;
        bloque_idx++;
    }
    
    marcarBitInodo(path_disco, sb.s_bm_inode_start, sb.s_inodes_count, nuevo_inodo_pos);
    if (!escribirInodo(path_disco, sb.s_inode_start, nuevo_inodo_pos, nuevo_inodo)) {
        return "ERROR: No se pudo escribir inodo";
    }
    
    // Agregar entrada en carpeta padre
    Inodo inodo_padre_obj;
    leerInodo(path_disco, sb.s_inode_start, inodo_padre, inodo_padre_obj);
    
    for (int i = 0; i < 12 && inodo_padre_obj.i_block[i] != -1; i++) {
        BloqueCarpeta bloque;
        if (leerBloqueCarpeta(path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque)) {
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo == -1) {
                    strcpy(bloque.b_content[j].b_name, nombre_archivo.c_str());
                    bloque.b_content[j].b_inodo = nuevo_inodo_pos;
                    escribirBloqueCarpeta(path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque);
                    return "OK";
                }
            }
        }
    }
    
    return "ERROR: No hay espacio en carpeta padre";
}

// --- FUNCION: crearCarpetasPadre ---
bool crearCarpetasPadre(string path_disco, Superblock &sb, string ruta, int uid, int gid) {
    if (ruta == "/") return true;
    
    vector<string> partes = obtenerRutaArray(ruta);
    string ruta_actual = "";
    
    for (const string& parte : partes) {
        ruta_actual += "/" + parte;
        
        // Verificar si ya existe
        int inodo_existente = buscarInodoPorRuta(path_disco, sb.s_inode_start, sb.s_block_start, ruta_actual);
        if (inodo_existente != -1) continue;
        
        // Crear la carpeta
        string resultado = mkdirInterno(path_disco, sb, ruta_actual, uid, gid);
        if (resultado.find("ERROR") != string::npos) {
            return false;
        }
    }
    return true;
}

// --- FUNCION: leerArchivoCompleto ---
string leerArchivoCompleto(string path_disco, Superblock &sb, string ruta) {
    // Buscar inodo del archivo
    int inodo_pos = buscarInodoPorRuta(path_disco, sb.s_inode_start, sb.s_block_start, ruta);
    if (inodo_pos == -1) {
        return "";  // Archivo no existe
    }
    
    Inodo inodo;
    if (!leerInodo(path_disco, sb.s_inode_start, inodo_pos, inodo)) {
        return "";
    }
    
    // Verificar que sea un archivo
    if (inodo.i_type != 1) {
        return "";  // No es archivo
    }
    
    // Verificar permisos de lectura
    
    string contenido = "";
    int bytes_por_leer = inodo.i_size;
    int pos = 0;
    
    // Leer bloques directos
    for (int i = 0; i < 12 && bytes_por_leer > 0 && inodo.i_block[i] != -1; i++) {
        BloqueArchivo bloque;
        if (!leerBloqueArchivo(path_disco, sb.s_block_start, inodo.i_block[i], bloque)) {
            continue;
        }
        
        int copiar = min(bytes_por_leer, 64);
        contenido += string(bloque.b_content, copiar);
        bytes_por_leer -= copiar;
    }
    
    // Leer bloques indirectos para archivos grandes
    
    return contenido;
}

// --- FUNCION: generarTreeRecursivo ---
void generarTreeRecursivo(ofstream &dot, string path_disco, Superblock &sb, int inodo_pos, int &nodo_count) {
    Inodo inodo;
    if (!leerInodo(path_disco, sb.s_inode_start, inodo_pos, inodo)) return;
    
    int mi_nodo = nodo_count++;
    
    // Crear nodo para el INODO
    dot << "  inodo" << mi_nodo << " [label=\"{Inodo " << inodo_pos << "|";
    dot << (inodo.i_type == 0 ? "Carpeta" : "Archivo") << "|";
    dot << "Size: " << inodo.i_size << "|";
    dot << "Perm: " << inodo.i_perm[0] << inodo.i_perm[1] << inodo.i_perm[2];
    dot << "}\"];" << endl;
    
    // Procesar cada bloque del inodo
    for (int i = 0; i < 15; i++) {
        if (inodo.i_block[i] == -1) continue;
        
        int bloque_pos = inodo.i_block[i];
        int bloque_nodo = nodo_count++;
        
        // Si es inodo de carpeta
        if (inodo.i_type == 0 && i < 12) {
            BloqueCarpeta bloque_carp;
            if (!leerBloqueCarpeta(path_disco, sb.s_block_start, bloque_pos, bloque_carp)) continue;
            
            dot << "  bloque" << bloque_nodo << " [label=\"{Bloque Carpeta " << bloque_pos << "}\"];" << endl;
            dot << "  inodo" << mi_nodo << " -> bloque" << bloque_nodo << ";" << endl;
            
            // Procesar hijos
            for (int j = 0; j < 4; j++) {
                if (bloque_carp.b_content[j].b_inodo != -1) {
                    string nombre(bloque_carp.b_content[j].b_name);
                    if (nombre != "." && nombre != "..") {
                        int hijo_inodo = bloque_carp.b_content[j].b_inodo;
                        int hijo_nodo = nodo_count;
                        generarTreeRecursivo(dot, path_disco, sb, hijo_inodo, nodo_count);
                        dot << "  bloque" << bloque_nodo << " -> inodo" << hijo_nodo;
                        dot << " [label=\"" << nombre << "\"];" << endl;
                    }
                }
            }
        }
        // Si es inodo de archivo
        else if (inodo.i_type == 1 && i < 12) {
            dot << "  bloque" << bloque_nodo << " [label=\"{Bloque Archivo " << bloque_pos << "}\"];" << endl;
            dot << "  inodo" << mi_nodo << " -> bloque" << bloque_nodo << ";" << endl;
        }
        // Si es bloque indirecto
        else if (i >= 12) {
            dot << "  bloque" << bloque_nodo << " [label=\"{Bloque Apuntadores " << bloque_pos << "}\"];" << endl;
            dot << "  inodo" << mi_nodo << " -> bloque" << bloque_nodo << ";" << endl;
        }
    }
}


// ******** FUNCIONES PARA MANIPULAR users.txt ********

// --- FUNCION: obtenerSuperblock ---
bool obtenerSuperblock(string path_disco, int part_start, Superblock &sb) {
    ifstream disco(path_disco, ios::binary);
    if (!disco.is_open()) return false;
    
    disco.seekg(part_start);
    disco.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    disco.close();
    return true;
}

// --- FUNCION: leerArchivo ---
string leerArchivo(string path_disco, int start_inodos, int start_bloques, int start_bm_bloques, int total_bloques, string ruta) {
    // Buscar inodo del archivo
    int inodo_pos = buscarInodoPorRuta(path_disco, start_inodos, start_bloques, ruta);
    if (inodo_pos == -1) return "";
    
    Inodo inodo;
    if (!leerInodo(path_disco, start_inodos, inodo_pos, inodo)) return "";
    
    string contenido = "";
    
    // Leer bloques directos
    for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
        BloqueArchivo bloque;
        if (leerBloqueArchivo(path_disco, start_bloques, inodo.i_block[i], bloque)) {
            // LEER CARACTER POR CARACTER hasta encontrar \0 o 64
            for (int j = 0; j < 64; j++) {
                if (bloque.b_content[j] == '\0') break;
                // Solo agregar caracteres imprimibles y saltos de línea
                if (isprint(bloque.b_content[j]) || bloque.b_content[j] == '\n') {
                    contenido += bloque.b_content[j];
                }
            }
        }
    }
    
    return contenido;
}

// --- FUNCION: escribirArchivo ---
bool escribirArchivo(string path_disco, int start_inodos, int start_bloques, int start_bm_inodos, int start_bm_bloques, 
                     int total_inodos, int total_bloques, string ruta, string contenido, int uid, int gid) {
    // Buscar inodo del archivo
    int inodo_pos = buscarInodoPorRuta(path_disco, start_inodos, start_bloques, ruta);
    if (inodo_pos == -1) return false;
    
    Inodo inodo;
    if (!leerInodo(path_disco, start_inodos, inodo_pos, inodo)) return false;
    
    // Liberar bloques anteriores
    for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
        liberarBitBloque(path_disco, start_bm_bloques, total_bloques, inodo.i_block[i]);
        inodo.i_block[i] = -1;
    }
    
    // Escribir nuevo contenido
    int bytes_restantes = contenido.length();
    int bloque_actual = 0;
    int pos = 0;
    
    while (bytes_restantes > 0 && bloque_actual < 12) {
        // Buscar bloque libre
        int bloque_libre = obtenerBloqueLibre(path_disco, start_bm_bloques, total_bloques);
        if (bloque_libre == -1) return false;
        
        inodo.i_block[bloque_actual] = bloque_libre;
        marcarBitBloque(path_disco, start_bm_bloques, total_bloques, bloque_libre);
        
        BloqueArchivo bloque;
        int copiar = min(bytes_restantes, 64);
        memset(bloque.b_content, 0, 64);
        strncpy(bloque.b_content, contenido.substr(pos, copiar).c_str(), copiar);
        
        if (!escribirBloqueArchivo(path_disco, start_bloques, inodo.i_block[bloque_actual], bloque)) {
            return false;
        }
        
        bytes_restantes -= copiar;
        pos += copiar;
        bloque_actual++;
    }
    
    inodo.i_size = contenido.length();
    return escribirInodo(path_disco, start_inodos, inodo_pos, inodo);
}

// --- FUNCION: crearCarpetaSiNoExiste ---
void crearCarpetaSiNoExiste(string path) {
    if (path.empty()) return;  // Si la ruta esta vacia, no hacer nada
    
    size_t pos = path.find_last_of('/');
    if (pos != string::npos && pos > 0) {
        string carpeta = path.substr(0, pos);
        if (!carpeta.empty()) {
            string comando = "mkdir -p \"" + carpeta + "\"";
            system(comando.c_str());
        }
    }
}

// --- FUNCION: registrarEnJournal ---
void registrarEnJournal(string path_disco, int part_start, string operacion, string ruta, string contenido) {
    Superblock sb;
    if (!obtenerSuperblock(path_disco, part_start, sb)) return;
    
    if (sb.s_filesystem_type != 3) return; // Solo EXT3
    
    fstream disco(path_disco, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) return;
    
    // Buscar la primera entrada de journal libre
    for (int i = 0; i < sb.s_inodes_count; i++) {
        Journal journal;
        disco.seekg(part_start + sizeof(Superblock) + (i * sizeof(Journal)));
        disco.read(reinterpret_cast<char*>(&journal), sizeof(Journal));
        
        if (journal.j_count == 0) {
            // Entrada libre encontrada
            journal.j_count = 1;
            strncpy(journal.j_content.i_operation, operacion.c_str(), 9);
            strncpy(journal.j_content.i_path, ruta.c_str(), 31);
            strncpy(journal.j_content.i_content, contenido.c_str(), 63);
            journal.j_content.i_date = time(nullptr);
            
            disco.seekp(part_start + sizeof(Superblock) + (i * sizeof(Journal)));
            disco.write(reinterpret_cast<const char*>(&journal), sizeof(Journal));
            
            cout << "JOURNAL: " << operacion << " | " << ruta << " | " << contenido << endl;
            
            break;
        }
    }
    
    disco.close();
}

// --- FUNCION: mkdisk (para crear disco virtual) ---
string mkdisk(int size, string unit, string fit, string path) {
    
    // VALIDAR QUE LA RUTA NO ESTÉ VACÍA
    if (path.empty()) {
        return "ERROR: Ruta no especificada";
    }
    
    // CREAR CARPETA SI NO EXISTE
    crearCarpetaSiNoExiste(path);
    
    // VALIDACION DE PARAMETROS
    // Verificar que el tamaño sea positivo
    if (size <= 0) {
        return "ERROR: El tamaño debe ser mayor a 0";
    }
    
    // CONVERSION DE UNIDADES A BYTES
    int total_bytes = 0;
    
    if (unit == "K") {
        total_bytes = size * 1024;          // Kilobytes
    } 
    else if (unit == "M") {
        total_bytes = size * 1024 * 1024;   // Megabytes
    }
    else {
        // Si no se da la unidad se usara Megabytes
        total_bytes = size * 1024 * 1024;
    }
    
    // CREACION DEL ARCHIVO .mia
    // Abrir el archivo binario para escritura
    ofstream archivo(path, ios::binary);

    // Verificar si se pudo crear el archivo
    if (!archivo.is_open()) {
        return "ERROR: NO se pudo crear el archivo en la ruta: " + path;
    }
    
    // LLENAR EL ARCHIVO CON CEROS
    char buffer[1024] = {0};        // Buffer lleno de ceros
    int escritos = 0;

    while (escritos < total_bytes) {
        // Calcular cuantos bytes escribir
        int chunk = min(1024, total_bytes - escritos);
        archivo.write(buffer, chunk);
        escritos += chunk;
    }
    
    // CREAR Y ESCRIBIR EL MBR
    MBR nuevo_mbr;
    nuevo_mbr.mbr_size = total_bytes;
    
    // Mover el puntero al inicio del archivo y escribir el MBR
    archivo.seekp(0);
    archivo.write(reinterpret_cast<const char*>(&nuevo_mbr), sizeof(MBR));
    archivo.close();
    
    // MENSAJE DE EXITO
    ostringstream res;
    res << "MKDISK: Disco creado exitosamente\n";
    res << "Ruta: " << path << "\n";
    res << "Tamaño: " << total_bytes << " bytes (" << size << " " << unit << ")\n";
    res << "Fit: " << fit << "\n";
    res << "Signature: " << nuevo_mbr.mbr_dsk_signature;
    return res.str();
}

// --- FUNCION: rmdisk (eliminar disco) ---
string rmdisk(string path) {
    // Verificar que el archivo existe
    ifstream archivo(path);
    if (!archivo.is_open()) {
        return "ERROR: El disco no existe en la ruta: " + path;
    }
    archivo.close();
    
    // Eliminar el archivo
    if (remove(path.c_str()) != 0) {
        return "ERROR: No se pudo eliminar el disco";
    }
    
    return "RMDISK: Disco eliminado exitosamente\nRuta: " + path;
}

// --- FUNCION: fdisk (crear/eliminar/redimensionar particion) ---
string fdisk(int size, string unit, string path, string type, string fit, string name, string delete_type, int add_size) {

    // 1. Leer MBR del disco
    MBR mbr;
    if (!leerMBR(path, mbr)) {
        return "ERROR: No se pudo leer el disco en " + path;
    }

    // ELIMINAR PARTICION (si viene -delete
    if (!delete_type.empty()) {
        // Buscar particion por nombre
        int idx = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_size > 0) {
                string nombre_existente(mbr.mbr_partitions[i].part_name);
                if (nombre_existente == name) {
                    idx = i;
                    break;
                }
            }
        }
        
        if (idx == -1) {
            return "ERROR: No existe particion con nombre '" + name + "'";
        }
        
        // Si es extendida, limpiar todo su espacio
        if (mbr.mbr_partitions[idx].part_type == 'E') {
            ofstream disco(path, ios::binary | ios::in | ios::out);
            if (disco.is_open()) {
                char* ceros = new char[mbr.mbr_partitions[idx].part_size]();
                disco.seekp(mbr.mbr_partitions[idx].part_start);
                disco.write(ceros, mbr.mbr_partitions[idx].part_size);
                delete[] ceros;
                disco.close();
            }
        }
        
        if (delete_type == "full") {
            ofstream disco(path, ios::binary | ios::in | ios::out);
            if (disco.is_open()) {
                char* ceros = new char[mbr.mbr_partitions[idx].part_size]();
                disco.seekp(mbr.mbr_partitions[idx].part_start);
                disco.write(ceros, mbr.mbr_partitions[idx].part_size);
                delete[] ceros;
                disco.close();
            }
        }
        
        // Marcar particion como vacia
        mbr.mbr_partitions[idx].part_size = 0;
        mbr.mbr_partitions[idx].part_start = 0;
        memset(mbr.mbr_partitions[idx].part_name, 0, 16);
        mbr.mbr_partitions[idx].part_status = '0';
        mbr.mbr_partitions[idx].part_correlative = -1;
        
        if (!escribirMBR(path, mbr)) {
            return "ERROR: No se pudo actualizar el MBR";
        }
        
        return "FDISK: Particion '" + name + "' eliminada (" + delete_type + ")";
    }
    
    // AGREGAR O QUITAR ESPACIO (si viene -add)
    if (add_size != 0) {
        // Buscar particion por nombre
        int idx = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_size > 0) {
                string nombre_existente(mbr.mbr_partitions[i].part_name);
                if (nombre_existente == name) {
                    idx = i;
                    break;
                }
            }
        }
        
        if (idx == -1) {
            return "ERROR: No existe particion con nombre '" + name + "'";
        }
        
        int nuevo_tamano = mbr.mbr_partitions[idx].part_size + add_size;
        
        if (nuevo_tamano <= 0) {
            return "ERROR: La particion quedaria con tamaño negativo o cero";
        }
        
        // Validar que no se salga del disco
        int fin_particion = mbr.mbr_partitions[idx].part_start + nuevo_tamano;
        if (fin_particion > mbr.mbr_size) {
            return "ERROR: La particion excede el tamaño del disco";
        }
        
        // Validar que no invada otra particion
        for (int i = 0; i < 4; i++) {
            if (i != idx && mbr.mbr_partitions[i].part_size > 0) {
                int inicio_otra = mbr.mbr_partitions[i].part_start;
                int fin_otra = inicio_otra + mbr.mbr_partitions[i].part_size;
                
                if (add_size > 0) {
                    if (fin_particion > inicio_otra) {
                        return "ERROR: No hay espacio libre despues de la particion";
                    }
                } else {
                    if (mbr.mbr_partitions[idx].part_start < fin_otra && 
                        fin_particion > inicio_otra) {
                        return "ERROR: No se puede reducir porque invadiria otra particion";
                    }
                }
            }
        }
        
        // Actualizar tamaño
        mbr.mbr_partitions[idx].part_size = nuevo_tamano;
        
        if (!escribirMBR(path, mbr)) {
            return "ERROR: No se pudo actualizar el MBR";
        }
        
        string operacion = (add_size > 0) ? "agregados" : "quitados";
        return "FDISK: " + to_string(abs(add_size)) + " bytes " + operacion + " a particion '" + name + "'. Nuevo tamaño: " + to_string(nuevo_tamano) + " bytes";
    }

    // CREAR PARTICION NUEVA
    
    // 2. Validar tamaño (solo para crear)
    if (size <= 0) return "ERROR: Tamaño invalido";

    // 3. Convertir a bytes
    int tamano_bytes = size;
    if (unit == "K") tamano_bytes = size * 1024;
    else if (unit == "M") tamano_bytes = size * 1024 * 1024;
    else tamano_bytes = size * 1024;

    // 4. Validar que no haya una particion con el mismo nombre
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            string nombre_existente(mbr.mbr_partitions[i].part_name);
            if (nombre_existente == name) {
                return "ERROR: Ya existe una particion con el nombre '" + name + "'";
            }
        }
    }

    // 5. Contar particiones existentes
    int count = 0;
    int extendida_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            count++;
            if (mbr.mbr_partitions[i].part_type == 'E') extendida_idx = i;
        }
    }

    // 6. Validar limites
    if (count >= 4 && type != "L") return "ERROR: El maximo de particiones es de 4";

    // 7. Si es extendida -> validar que no haya otra
    if (type == "E" && extendida_idx != -1) {
        return "ERROR: Ya existe una particion extendida";
    }

    // 8. Si es logica -> validar que exista extendida
    if (type == "L" && extendida_idx == -1) {
        return "ERROR: No hay particion extendida para crear logicas";
    }

    // EN CASO DE PARTICION LOGICA
    if (type == "L") {
        int ext_start = mbr.mbr_partitions[extendida_idx].part_start;
        int ext_size = mbr.mbr_partitions[extendida_idx].part_size;

        int ebr_pos = ext_start;
        EBR ebr_actual;
        int last_ebr_pos = -1;
        int last_ebr_end = ext_start;

        while (ebr_pos != -1 && ebr_pos < ext_start + ext_size) {
            if (!leerEBR(path, ebr_pos, ebr_actual)) break;
            last_ebr_pos = ebr_pos;
            last_ebr_end = ebr_actual.part_start + ebr_actual.part_size;
            ebr_pos = ebr_actual.part_next;
        }

        int espacio_disponible = (ext_start + ext_size) - last_ebr_end;
        int espacio_necesario = tamano_bytes + sizeof(EBR);

        if (espacio_disponible < espacio_necesario) {
            return "ERROR: No hay espacio suficiente en la particion extendida";
        }

        int nuevo_ebr_pos = last_ebr_end;
        EBR nuevo_ebr;
        nuevo_ebr.part_mount = '0';
        nuevo_ebr.part_fit = fit[0];
        nuevo_ebr.part_start = nuevo_ebr_pos + sizeof(EBR);
        nuevo_ebr.part_size = tamano_bytes;
        nuevo_ebr.part_next = -1;
        strcpy(nuevo_ebr.part_name, name.c_str());

        if (!escribirEBR(path, nuevo_ebr_pos, nuevo_ebr)) {
            return "ERROR: No se pudo escribir el EBR";
        }

        if (last_ebr_pos != -1) {
            ebr_actual.part_next = nuevo_ebr_pos;
            if (!escribirEBR(path, last_ebr_pos, ebr_actual)) {
                return "ERROR: No se pudo actualizar el EBR anterior";
            }
        }

        stringstream res;
        res << "FDISK: Particion logica creada exitosamente\n";
        res << "Disco: " << path << "\n";
        res << "Nombre: " << name << "\n";
        res << "Tipo: " << type << "\n";
        res << "Tamaño datos: " << tamano_bytes << " bytes (" << size << " " << unit << ")\n";
        res << "EBR en: " << nuevo_ebr_pos << "\n";
        res << "Datos inician en: " << nuevo_ebr.part_start;
        return res.str();
    }

    // EN CASO DE PARTICION PRIMARIA O EXTENDIDA

    // 9. Calcular espacios libres
    char fit_char = fit[0];
    vector<pair<int, int>> espacios = calcularEspaciosLibres(mbr, mbr.mbr_size);

    // 10. Elegir el espacio segun el ajuste
    int idx_espacio = elegirAjuste(espacios, tamano_bytes, fit_char);
    if (idx_espacio == -1) {
        return "ERROR: No hay espacio suficiente para la particion";
    }

    // 11. Crear nueva particion
    Partition nueva;
    nueva.part_status = '0';
    nueva.part_type = type[0];
    nueva.part_fit = fit_char;
    nueva.part_start = espacios[idx_espacio].first;
    nueva.part_size = tamano_bytes;
    strcpy(nueva.part_name, name.c_str());
    nueva.part_correlative = -1;
    memset(nueva.part_id, 0, 4);

    // 12. Buscar slot libre en MBR
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size == 0) {
            mbr.mbr_partitions[i] = nueva;
            break;
        }
    }

    // 13. Si es extendida -> crear el primer EBR
    if (type == "E") {
        EBR ebr_inicial;
        ebr_inicial.part_mount = '0';
        ebr_inicial.part_fit = fit[0];
        ebr_inicial.part_start = nueva.part_start + sizeof(EBR);
        ebr_inicial.part_size = 0;
        ebr_inicial.part_next = -1;
        strcpy(ebr_inicial.part_name, "EBR_Inicial");

        if (!escribirEBR(path, nueva.part_start, ebr_inicial)) {
            return "ERROR: No se pudo crear EBR inicial";
        }
    }

    // 14. Guardar MBR actualizado
    if (!escribirMBR(path, mbr)) {
        return "ERROR: No se pudo guardar el MBR";
    }

    // 15. Mensaje de exito
    stringstream res;
    res << "FDISK: Particion creada exitosamente\n";
    res << "Disco: " << path << "\n";
    res << "Nombre: " << name << "\n";
    res << "Tipo: " << type << "\n";
    res << "Tamaño: " << tamano_bytes << " bytes (" << size << " " << unit << ")\n";
    res << "Ajuste: " << fit << "\n";
    res << "Inicio: " << nueva.part_start;
    return res.str();
}

// --- COMANDO: mount (montar particion) ---
string mount(string path, string name) {
    MBR mbr;
    if (!leerMBR(path, mbr)) {
        return "ERROR: No se pudo leer el disco en " + path;
    }
    
    int idx = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            string nombre_actual(mbr.mbr_partitions[i].part_name);
            if (nombre_actual == name) {
                idx = i;
                break;
            }
        }
    }
    
    if (idx == -1) {
        return "ERROR: No existe una particion con nombre '" + name + "'";
    }
    
    for (const auto& m : particiones_montadas) {
        if (m.path_disco == path && m.nombre_particion == name) {
            return "ERROR: La particion ya esta montada";
        }
    }
    
    string id_generado = generarID(path);
    
    mbr.mbr_partitions[idx].part_status = '1';
    strcpy(mbr.mbr_partitions[idx].part_id, id_generado.c_str());
    
    if (!escribirMBR(path, mbr)) {
        return "ERROR: No se pudo actualizar el MBR";
    }
    
    Montada nueva;
    nueva.path_disco = path;
    nueva.nombre_particion = name;
    nueva.id = id_generado;
    nueva.part_start = mbr.mbr_partitions[idx].part_start;
    nueva.part_size = mbr.mbr_partitions[idx].part_size;
    nueva.part_type = mbr.mbr_partitions[idx].part_type;
    
    particiones_montadas.push_back(nueva);
    
    return "MOUNT: Particion montada exitosamente\nID: " + id_generado;
}

// --- COMANDO: unmount (desmontar particion) ---
string unmount(string id) {
    // Buscar la particion por ID
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return "ERROR: No existe particion montada con ID: " + id;
    }
    
    Montada& m = particiones_montadas[idx];
    
    // Leer el MBR del disco
    MBR mbr;
    if (!leerMBR(m.path_disco, mbr)) {
        return "ERROR: No se pudo leer el disco";
    }
    
    // Buscar la particion por nombre y actualizar su estado
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            string nombre_existente(mbr.mbr_partitions[i].part_name);
            if (nombre_existente == m.nombre_particion) {
                mbr.mbr_partitions[i].part_status = '0';
                mbr.mbr_partitions[i].part_correlative = -1;
                memset(mbr.mbr_partitions[i].part_id, 0, 4);
                break;
            }
        }
    }
    
    // Guardar MBR actualizado
    if (!escribirMBR(m.path_disco, mbr)) {
        return "ERROR: No se pudo actualizar el MBR";
    }
    
    // Eliminar de la lista de montadas
    particiones_montadas.erase(particiones_montadas.begin() + idx);
    
    return "UNMOUNT: Particion " + id + " desmontada exitosamente";
}

// --- COMANDO: mounted (particiones montadas en memoria) ---
string mounted() {
    // Verificar si hay particiones montadas
    if (particiones_montadas.empty()) {
        return "MOUNTED: No hay particiones montadas";
    }
    
    // Crear el string con el resultado
    stringstream res;
    res << "=== PARTICIONES MONTADAS ===\n";
    
    // Recorrer todas las particiones montadas
    for (const auto& m : particiones_montadas) {
        res << "ID: " << m.id << "\n";
        res << "  Disco: " << m.path_disco << "\n";
        res << "  Particion: " << m.nombre_particion << "\n";
        res << "  Tipo: " << m.part_type << "\n";
        res << "  Inicio: " << m.part_start << "\n";
        res << "  Tamaño: " << m.part_size << " bytes\n\n";
    }
    
    return res.str();
}

// --- FUNCION: mkfs (formatear particion y creacion del archivo raiz 'users.txt') ---
string mkfs(string id, string type, string fs) {
    // 1. Buscar la particion montada por ID
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return "ERROR: No existe particion montada con ID: " + id;
    }
    
    Montada& m = particiones_montadas[idx];
    
    // 2. Abrir disco
    fstream disco(m.path_disco, ios::binary | ios::in | ios::out);
    if (!disco.is_open()) {
        return "ERROR: No se pudo abrir el disco";
    }
    
    // 3. Determinar sistema de archivos
    bool esExt3 = (fs == "3fs");
    
    // 4. Calcular el numero de inodos/bloques (n)
    int n = 0;
    int superblock_size = sizeof(Superblock);
    int journal_size = sizeof(Journal);
    
    if (esExt3) {
        // Formula EXT3: superblock + n*journal + n + 3n + n*sizeof(Inodo) + 3n*64
        while (true) {
            int necesario = superblock_size + (n * journal_size) + n + (3 * n) + (n * sizeof(Inodo)) + (3 * n * 64);
            if (necesario <= m.part_size) {
                n++;
            } else {
                break;
            }
        }
        n = n - 1;
    } else {
        // Formula EXT2: superblock + n + 3n + n*sizeof(Inodo) + 3n*64
        while (true) {
            int necesario = superblock_size + n + (3 * n) + (n * sizeof(Inodo)) + (3 * n * 64);
            if (necesario <= m.part_size) {
                n++;
            } else {
                break;
            }
        }
        n = n - 1;
    }
    
    // 5. Posicionarse al inicio de la particion
    disco.seekp(m.part_start);
    
    // 6. Escribir Superbloque
    Superblock sb;
    sb.s_filesystem_type = esExt3 ? 3 : 2;
    sb.s_inodes_count = n;
    sb.s_blocks_count = 3 * n;
    sb.s_free_inodes_count = n;
    sb.s_free_blocks_count = 3 * n;
    sb.s_mtime = time(nullptr);
    sb.s_umtime = time(nullptr);
    sb.s_mnt_count = 0;
    sb.s_magic = 0xEF53;
    sb.s_inode_size = sizeof(Inodo);
    sb.s_block_size = 64;
    sb.s_first_inode = 0;
    sb.s_first_block = 0;
    
    if (esExt3) {
        sb.s_bm_inode_start = m.part_start + sizeof(Superblock) + (n * sizeof(Journal));
    } else {
        sb.s_bm_inode_start = m.part_start + sizeof(Superblock);
    }
    sb.s_bm_block_start = sb.s_bm_inode_start + n;
    sb.s_inode_start = sb.s_bm_block_start + (3 * n);
    sb.s_block_start = sb.s_inode_start + (n * sizeof(Inodo));
    
    disco.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
    
    // 7. Si es EXT3, inicializar journal
    if (esExt3) {
        Journal* journals = new Journal[n]();
        for (int i = 0; i < n; i++) {
            disco.write(reinterpret_cast<const char*>(&journals[i]), sizeof(Journal));
        }
        delete[] journals;
    }
    
    // 8. Inicializar bitmap de inodos (todos en 0)
    char* bm_inodos = new char[n]();
    disco.write(bm_inodos, n);
    
    // 9. Inicializar bitmap de bloques (todos en 0)
    char* bm_bloques = new char[3 * n]();
    disco.write(bm_bloques, 3 * n);
    
    // 10. Inicializar tabla de inodos (todos vacios)
    Inodo* inodos = new Inodo[n]();
    for (int i = 0; i < n; i++) {
        disco.write(reinterpret_cast<const char*>(&inodos[i]), sizeof(Inodo));
    }
    
    // 11. Inicializar bloques (todos vacios)
    char bloque_vacio[64] = {0};
    for (int i = 0; i < 3 * n; i++) {
        disco.write(bloque_vacio, 64);
    }
    
    // 12. Crear inodo raiz (carpeta /)
    disco.seekp(sb.s_inode_start);
    Inodo inodo_raiz;
    inodo_raiz.i_uid = 1;
    inodo_raiz.i_gid = 1;
    inodo_raiz.i_size = 0;
    inodo_raiz.i_atime = time(nullptr);
    inodo_raiz.i_ctime = time(nullptr);
    inodo_raiz.i_mtime = time(nullptr);
    inodo_raiz.i_type = 0;
    inodo_raiz.i_perm[0] = '7';
    inodo_raiz.i_perm[1] = '7';
    inodo_raiz.i_perm[2] = '7';
    inodo_raiz.i_block[0] = 0;
    for (int i = 1; i < 15; i++) inodo_raiz.i_block[i] = -1;
    disco.write(reinterpret_cast<const char*>(&inodo_raiz), sizeof(Inodo));
    
    // 13. Actualizar bitmap de inodos (inodo 0 ocupado)
    disco.seekp(sb.s_bm_inode_start);
    char primer_inodo = 1;
    disco.write(&primer_inodo, 1);
    
    // 14. Crear bloque de carpeta para raiz
    disco.seekp(sb.s_block_start);
    BloqueCarpeta bloque_raiz;
    memset(&bloque_raiz, 0, sizeof(BloqueCarpeta));
    
    // Entrada 0: "." (esta carpeta)
    strcpy(bloque_raiz.b_content[0].b_name, ".");
    bloque_raiz.b_content[0].b_inodo = 0;
    
    // Entrada 1: ".." (carpeta padre)
    strcpy(bloque_raiz.b_content[1].b_name, "..");
    bloque_raiz.b_content[1].b_inodo = 0;
    
    // Entrada 2: vacía (para users.txt)
    bloque_raiz.b_content[2].b_inodo = -1;
    memset(bloque_raiz.b_content[2].b_name, 0, 12);
    
    // Entrada 3: vacía
    bloque_raiz.b_content[3].b_inodo = -1;
    memset(bloque_raiz.b_content[3].b_name, 0, 12);
    
    disco.write(reinterpret_cast<const char*>(&bloque_raiz), sizeof(BloqueCarpeta));
    
    // 15. Actualizar bitmap de bloques (bloque 0 ocupado)
    disco.seekp(sb.s_bm_block_start);
    char primer_bloque = 1;
    disco.write(&primer_bloque, 1);
    
    // 16. Crear archivo users.txt
    string users_content = "1,G,root\n1,U,root,root,123\n";
    
    // Usar inodo 1 para users.txt
    int users_inodo_pos = 1;
    int users_bloque = 1;
    
    // Crear inodo para users.txt
    Inodo users_inodo;
    users_inodo.i_uid = 1;
    users_inodo.i_gid = 1;
    users_inodo.i_size = users_content.length();
    users_inodo.i_atime = time(nullptr);
    users_inodo.i_ctime = time(nullptr);
    users_inodo.i_mtime = time(nullptr);
    users_inodo.i_type = 1;  // Archivo
    users_inodo.i_perm[0] = '6';
    users_inodo.i_perm[1] = '6';
    users_inodo.i_perm[2] = '4';
    for (int i = 0; i < 15; i++) users_inodo.i_block[i] = -1;
    users_inodo.i_block[0] = users_bloque;
    
    // Marcar bloque como ocupado
    disco.seekp(sb.s_bm_block_start + users_bloque);
    char bit = 1;
    disco.write(&bit, 1);
    
    // Escribir contenido en el bloque
    BloqueArchivo bloque_users;
    memset(bloque_users.b_content, 0, 64);
    strncpy(bloque_users.b_content, users_content.c_str(), users_content.length());
    
    disco.seekp(sb.s_block_start + users_bloque * 64);
    disco.write(reinterpret_cast<const char*>(&bloque_users), sizeof(BloqueArchivo));
    
    // Marcar inodo como ocupado
    disco.seekp(sb.s_bm_inode_start + users_inodo_pos);
    bit = 1;
    disco.write(&bit, 1);
    
    // Escribir inodo
    disco.seekp(sb.s_inode_start + users_inodo_pos * sizeof(Inodo));
    disco.write(reinterpret_cast<const char*>(&users_inodo), sizeof(Inodo));
    
    // Actualizar la raiz: agregar users.txt en la entrada 2
    disco.seekp(sb.s_block_start);
    BloqueCarpeta bloque_raiz_actual;
    disco.read(reinterpret_cast<char*>(&bloque_raiz_actual), sizeof(BloqueCarpeta));
    
    strcpy(bloque_raiz_actual.b_content[2].b_name, "users.txt");
    bloque_raiz_actual.b_content[2].b_inodo = users_inodo_pos;
    
    disco.seekp(sb.s_block_start);
    disco.write(reinterpret_cast<const char*>(&bloque_raiz_actual), sizeof(BloqueCarpeta));
    
    disco.close();
    delete[] bm_inodos;
    delete[] bm_bloques;
    delete[] inodos;
    
    string fs_nombre = esExt3 ? "EXT3" : "EXT2";
    return "MKFS: Particion formateada con " + fs_nombre + " exitosamente";
}

// ******** VARIABLES GLOBALES DE SESION ********
struct Sesion {
    bool activa;
    string id_particion;
    string usuario;
    int uid;
    int gid;
    
    Sesion() {
        activa = false;
        id_particion = "";
        usuario = "";
        uid = -1;
        gid = -1;
    }
};

Sesion sesion_actual;

// --- FUNCION: login ---
string login(string user, string pass, string id) {
    try {
        // 1. Verificar que no haya sesion activa
        if (sesion_actual.activa) {
            return "ERROR: Ya hay una sesion activa. Debe cerrar sesion primero";
        }
        
        // 2. Buscar particion montada por ID
        int idx = -1;
        for (int i = 0; i < particiones_montadas.size(); i++) {
            if (particiones_montadas[i].id == id) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) {
            return "ERROR: No existe particion montada con ID: " + id;
        }
        
        Montada& m = particiones_montadas[idx];
        
        // 3. Obtener superbloque
        Superblock sb;
        if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
            return "ERROR: No se pudo leer superbloque";
        }
        
        // 4. Leer users.txt
        string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                                   sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
        
        // 5. Buscar usuario en el archivo
        stringstream ss(users);
        string linea;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            // Formato: UID,U,GID,USER,PASS
            if (partes.size() >= 5 && partes[1] == "U" && partes[3] == user && partes[4] == pass) {
                sesion_actual.activa = true;
                sesion_actual.id_particion = id;
                sesion_actual.usuario = user;
                try {
                    sesion_actual.uid = stoi(partes[0]);
                    sesion_actual.gid = stoi(partes[2]);
                } catch (...) {
                    return "ERROR: Error al leer datos del usuario";
                }
                return "LOGIN: Sesion iniciada como " + user;
            }
        }
        
        return "ERROR: Usuario o contraseña incorrectos";
    } catch (const std::exception& e) {
        return "ERROR: Excepción en login: " + string(e.what());
    }
}

// --- FUNCION: logout ---
string logout() {
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    sesion_actual = Sesion();  // Reiniciar
    return "LOGOUT: Sesion cerrada correctamente";
}

// --- FUNCION: MKGRP (crea un grupo para los usuarios de la particion) ---
string mkgrp(string name) {
    try {
        // 1. Verificar sesion activa y root
        if (!sesion_actual.activa) {
            return "ERROR: No hay sesion activa";
        }
        
        if (sesion_actual.usuario != "root") {
            return "ERROR: Solo root puede crear grupos";
        }
        
        // 2. Validar longitud (max 10 caracteres)
        if (name.length() >= 11) {
            return "ERROR: El nombre del grupo no puede exceder 10 caracteres";
        }
        
        // 3. Buscar particion montada
        int idx = -1;
        for (int i = 0; i < particiones_montadas.size(); i++) {
            if (particiones_montadas[i].id == sesion_actual.id_particion) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) return "ERROR: Particion no encontrada";
        
        Montada& m = particiones_montadas[idx];
        
        // 4. Obtener superbloque
        Superblock sb;
        if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
            return "ERROR: No se pudo leer superbloque";
        }
        
        // 5. Leer users.txt
        string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                                   sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
        
        // 6. Verificar si users.txt esta vacio
        if (users.empty()) {
            users = "1,G,root\n1,U,root,root,123\n";
        }
        
        // 7. Buscar ultimo GID usado
        int max_gid = 1;
        stringstream ss(users);
        string linea;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            if (partes.size() >= 2 && partes[1] == "G") {
                try {
                    int gid = stoi(partes[0]);
                    if (gid > max_gid) max_gid = gid;
                } catch (...) {
                    continue;
                }
            }
        }
        
        // 8. Verificar si el grupo ya existe
        ss.clear();
        ss.str(users);
        while (getline(ss, linea)) {
            if (linea.find(",G," + name) != string::npos) {
                return "ERROR: El grupo '" + name + "' ya existe";
            }
        }
        
        // 9. Agregar nuevo grupo
        int nuevo_gid = max_gid + 1;
        string nueva_linea = to_string(nuevo_gid) + ",G," + name + "\n";
        users += nueva_linea;
        
        // 10. Escribir users.txt actualizado
        if (!escribirArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                            sb.s_bm_inode_start, sb.s_bm_block_start,
                            sb.s_inodes_count, sb.s_blocks_count, 
                            "/users.txt", users, sesion_actual.uid, sesion_actual.gid)) {
            return "ERROR: No se pudo actualizar users.txt";
        }
        
        return "MKGRP: Grupo '" + name + "' creado con GID " + to_string(nuevo_gid);
    } catch (const std::exception& e) {
        return "ERROR: Excepción en mkgrp: " + string(e.what());
    }
}

// --- FUNCION: RMGRP (elimina un grupo para los usuarios de la particion) ---
string rmgrp(string name) {
    try {
        // 1. Verificar sesion activa y root
        if (!sesion_actual.activa) {
            return "ERROR: No hay sesion activa";
        }
        
        if (sesion_actual.usuario != "root") {
            return "ERROR: Solo root puede eliminar grupos";
        }
        
        // 2. No permitir eliminar grupo root
        if (name == "root") {
            return "ERROR: No se puede eliminar el grupo root";
        }
        
        // 3. Buscar particion montada
        int idx = -1;
        for (int i = 0; i < particiones_montadas.size(); i++) {
            if (particiones_montadas[i].id == sesion_actual.id_particion) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) return "ERROR: Particion no encontrada";
        
        Montada& m = particiones_montadas[idx];
        
        // 4. Obtener superbloque
        Superblock sb;
        if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
            return "ERROR: No se pudo leer superbloque";
        }
        
        // 5. Leer users.txt
        string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                                   sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
        
        // 6. Verificar si users.txt esta vacio
        if (users.empty()) {
            return "ERROR: No hay archivo users.txt";
        }
        
        // 7. Buscar el GID del grupo a eliminar
        stringstream ss(users);
        string linea;
        int gid_eliminar = -1;
        bool grupo_existe = false;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            // Formato: GID,G,NOMBRE
            if (partes.size() >= 3 && partes[1] == "G" && partes[2] == name) {
                grupo_existe = true;
                try {
                    gid_eliminar = stoi(partes[0]);
                } catch (...) {
                    continue;
                }
                break;
            }
        }
        
        if (!grupo_existe) {
            return "ERROR: Grupo '" + name + "' no existe";
        }
        
        // 8. Verificar que no haya usuarios en ese grupo
        ss.clear();
        ss.str(users);
        bool usuarios_en_grupo = false;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            // Formato: UID,U,GID,USER,PASS
            if (partes.size() >= 4 && partes[1] == "U") {
                try {
                    int gid_usuario = stoi(partes[2]);
                    if (gid_usuario == gid_eliminar) {
                        usuarios_en_grupo = true;
                        break;
                    }
                } catch (...) {
                    continue;
                }
            }
        }
        
        if (usuarios_en_grupo) {
            return "ERROR: No se puede eliminar el grupo porque tiene usuarios asignados";
        }
        
        // 9. Eliminar el grupo (reconstruir archivo sin la línea del grupo)
        ss.clear();
        ss.str(users);
        string nuevo_users = "";
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            // Si es el grupo a eliminar, no se agrega
            if (partes.size() >= 3 && partes[1] == "G" && partes[2] == name) {
                continue;
            }
            
            nuevo_users += linea + "\n";
        }
        
        // 10. Escribir users.txt actualizado
        if (!escribirArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                            sb.s_bm_inode_start, sb.s_bm_block_start,
                            sb.s_inodes_count, sb.s_blocks_count, 
                            "/users.txt", nuevo_users, sesion_actual.uid, sesion_actual.gid)) {
            return "ERROR: No se pudo actualizar users.txt";
        }
        
        return "RMGRP: Grupo '" + name + "' eliminado (GID " + to_string(gid_eliminar) + ")";
    } catch (const std::exception& e) {
        return "ERROR: Excepción en rmgrp: " + string(e.what());
    }
}

// --- FUNCION: MKUSR (crear usuario) ---
string mkusr(string user, string pass, string grp) {
    try {
        // 1. Verificar sesion activa y root
        if (!sesion_actual.activa) {
            return "ERROR: No hay sesion activa";
        }
        
        if (sesion_actual.usuario != "root") {
            return "ERROR: Solo root puede crear usuarios";
        }
        
        // 2. Validar longitud maxima (10 caracteres)
        if (user.length() >= 11) {
            return "ERROR: El nombre de usuario no puede exceder 10 caracteres";
        }
        if (pass.length() >= 11) {
            return "ERROR: La contraseña no puede exceder 10 caracteres";
        }
        if (grp.length() >= 11) {
            return "ERROR: El nombre del grupo no puede exceder 10 caracteres";
        }
        
        // 3. Buscar particion montada
        int idx = -1;
        for (int i = 0; i < particiones_montadas.size(); i++) {
            if (particiones_montadas[i].id == sesion_actual.id_particion) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) return "ERROR: Particion no encontrada";
        
        Montada& m = particiones_montadas[idx];
        
        // 4. Obtener superbloque
        Superblock sb;
        if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
            return "ERROR: No se pudo leer superbloque";
        }
        
        // 5. Leer users.txt
        string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                                   sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
        
        // 6. Verificar si users.txt está vacio
        if (users.empty()) {
            users = "1,G,root\n1,U,root,root,123\n";
        }
        
        // 7. Verificar que el grupo exista
        stringstream ss(users);
        string linea;
        bool grupo_existe = false;
        int gid_grupo = -1;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            if (partes.size() >= 3 && partes[1] == "G" && partes[2] == grp) {
                grupo_existe = true;
                try {
                    gid_grupo = stoi(partes[0]);
                } catch (...) {
                    continue;
                }
                break;
            }
        }
        
        if (!grupo_existe) {
            return "ERROR: El grupo '" + grp + "' no existe";
        }
        
        // 8. Verificar que el usuario no exista
        ss.clear();
        ss.str(users);
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            if (partes.size() >= 5 && partes[1] == "U" && partes[3] == user) {
                return "ERROR: El usuario '" + user + "' ya existe";
            }
        }
        
        // 9. Buscar ultimo UID usado
        int max_uid = 1;
        ss.clear();
        ss.str(users);
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            if (partes.size() >= 2 && partes[1] == "U") {
                try {
                    int uid = stoi(partes[0]);
                    if (uid > max_uid) max_uid = uid;
                } catch (...) {
                    continue;
                }
            }
        }
        
        // 10. Agregar nuevo usuario
        int nuevo_uid = max_uid + 1;
        string nueva_linea = to_string(nuevo_uid) + ",U," + to_string(gid_grupo) + "," + user + "," + pass + "\n";
        users += nueva_linea;
        
        // 11. Escribir users.txt actualizado
        if (!escribirArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                            sb.s_bm_inode_start, sb.s_bm_block_start,
                            sb.s_inodes_count, sb.s_blocks_count, 
                            "/users.txt", users, sesion_actual.uid, sesion_actual.gid)) {
            return "ERROR: No se pudo actualizar users.txt";
        }
        
        return "MKUSR: Usuario '" + user + "' creado con UID " + to_string(nuevo_uid);
    } catch (const std::exception& e) {
        return "ERROR: Excepción en mkusr: " + string(e.what());
    }
}

// --- FUNCION: RMUSR (eliminar usuario) ---
string rmusr(string user) {
    try {
        // 1. Verificar sesion activa y root
        if (!sesion_actual.activa) {
            return "ERROR: No hay sesion activa";
        }
        
        if (sesion_actual.usuario != "root") {
            return "ERROR: Solo root puede eliminar usuarios";
        }
        
        // 2. No permitir eliminar root
        if (user == "root") {
            return "ERROR: No se puede eliminar el usuario root";
        }
        
        // 3. Buscar particion montada
        int idx = -1;
        for (int i = 0; i < particiones_montadas.size(); i++) {
            if (particiones_montadas[i].id == sesion_actual.id_particion) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) return "ERROR: Particion no encontrada";
        
        Montada& m = particiones_montadas[idx];
        
        // 4. Obtener superbloque
        Superblock sb;
        if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
            return "ERROR: No se pudo leer superbloque";
        }
        
        // 5. Leer users.txt
        string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                                   sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
        
        // 6. Buscar y eliminar usuario
        stringstream ss(users);
        string linea;
        string nuevo_users = "";
        bool encontrado = false;
        int uid_eliminado = -1;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            // Formato: UID,U,GID,USER,PASS
            if (partes.size() >= 5 && partes[1] == "U" && partes[3] == user) {
                encontrado = true;
                try {
                    uid_eliminado = stoi(partes[0]);
                } catch (...) {
                    continue;
                }
                // No se agrega (eliminado)
            } else {
                nuevo_users += linea + "\n";
            }
        }
        
        if (!encontrado) {
            return "ERROR: Usuario '" + user + "' no existe";
        }
        
        // 7. Escribir users.txt actualizado
        if (!escribirArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                            sb.s_bm_inode_start, sb.s_bm_block_start,
                            sb.s_inodes_count, sb.s_blocks_count, 
                            "/users.txt", nuevo_users, sesion_actual.uid, sesion_actual.gid)) {
            return "ERROR: No se pudo actualizar users.txt";
        }
        
        return "RMUSR: Usuario '" + user + "' eliminado (UID " + to_string(uid_eliminado) + ")";
    } catch (const std::exception& e) {
        return "ERROR: Excepción en rmusr: " + string(e.what());
    }
}

// --- FUNCION: CHGRP (Cambia grupo de usuario) ---
string chgrp(string user, string grp) {
    try {
        // 1. Verificar sesion activa y root
        if (!sesion_actual.activa) {
            return "ERROR: No hay sesion activa";
        }
        
        if (sesion_actual.usuario != "root") {
            return "ERROR: Solo root puede cambiar el grupo de un usuario";
        }
        
        // 2. Buscar particion montada
        int idx = -1;
        for (int i = 0; i < particiones_montadas.size(); i++) {
            if (particiones_montadas[i].id == sesion_actual.id_particion) {
                idx = i;
                break;
            }
        }
        
        if (idx == -1) return "ERROR: Particion no encontrada";
        
        Montada& m = particiones_montadas[idx];
        
        // 3. Obtener superbloque
        Superblock sb;
        if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
            return "ERROR: No se pudo leer superbloque";
        }
        
        // 4. Leer users.txt
        string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                                   sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
        
        // 5. Verificar si users.txt esta vacio
        if (users.empty()) {
            return "ERROR: No hay archivo users.txt";
        }
        
        // 6. Verificar que el grupo exista y obtener su GID
        stringstream ss(users);
        string linea;
        bool grupo_existe = false;
        int nuevo_gid = -1;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            // Formato: GID,G,NOMBRE
            if (partes.size() >= 3 && partes[1] == "G" && partes[2] == grp) {
                grupo_existe = true;
                try {
                    nuevo_gid = stoi(partes[0]);
                } catch (...) {
                    continue;
                }
                break;
            }
        }
        
        if (!grupo_existe) {
            return "ERROR: El grupo '" + grp + "' no existe";
        }
        
        // 7. Buscar usuario y cambiar grupo
        ss.clear();
        ss.str(users);
        string nuevo_users = "";
        bool usuario_encontrado = false;
        
        while (getline(ss, linea)) {
            if (linea.empty()) continue;
            
            vector<string> partes;
            stringstream ss2(linea);
            string parte;
            while (getline(ss2, parte, ',')) {
                partes.push_back(parte);
            }
            
            // Formato: UID,U,GID,USER,PASS
            if (partes.size() >= 5 && partes[1] == "U" && partes[3] == user) {
                usuario_encontrado = true;
                // Solo cambiar el GID
                string uid_original = partes[0];
                string pass_original = partes[4];
                nuevo_users += uid_original + ",U," + to_string(nuevo_gid) + "," + partes[3] + "," + pass_original + "\n";
            } else {
                nuevo_users += linea + "\n";
            }
        }
        
        if (!usuario_encontrado) {
            return "ERROR: Usuario '" + user + "' no existe";
        }
        
        // 8. Escribir users.txt actualizado
        if (!escribirArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                            sb.s_bm_inode_start, sb.s_bm_block_start,
                            sb.s_inodes_count, sb.s_blocks_count, 
                            "/users.txt", nuevo_users, sesion_actual.uid, sesion_actual.gid)) {
            return "ERROR: No se pudo actualizar users.txt";
        }
        
        return "CHGRP: Usuario '" + user + "' ahora pertenece al grupo '" + grp + "'";
    } catch (const std::exception& e) {
        return "ERROR: Excepción en chgrp: " + string(e.what());
    }
}

// --- FUNCION: MKFILE (crear archivo) ---
string mkfile(string path, bool r, int size, string cont) {
    // Verificar sesion activa
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    // Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Separar ruta en padre y nombre
    auto [ruta_padre, nombre_archivo] = obtenerPadreYNombre(path);
    
    // Verificar si existe el archivo
    int inodo_existente = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path);
    if (inodo_existente != -1) {
        return "ERROR: El archivo ya existe. Desea sobrescribir?";
    }
    
    // Verificar que existe la carpeta padre
    int inodo_padre = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre);
    
    // Si no existe y esta -r, crear carpetas padre
    if (inodo_padre == -1 && r) {
        if (!crearCarpetasPadre(m.path_disco, sb, ruta_padre, sesion_actual.uid, sesion_actual.gid)) {
            return "ERROR: No se pudieron crear las carpetas padre";
        }
        inodo_padre = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre);
    }
    
    if (inodo_padre == -1) {
        return "ERROR: La carpeta padre no existe (use -r para crearla)";
    }
    
    // Verificar permisos de escritura en carpeta padre
    
    // Obtener contenido del archivo
    string contenido;
    
    if (!cont.empty()) {
        // Leer de archivo en disco real
        ifstream archivo_origen(cont.c_str());
        if (!archivo_origen.is_open()) {
            return "ERROR: No se pudo leer el archivo de origen: " + cont;
        }
        stringstream buffer;
        buffer << archivo_origen.rdbuf();
        contenido = buffer.str();
        archivo_origen.close();
    } else if (size > 0) {
        // Generar contenido numerico 0-9
        for (int i = 0; i < size; i++) {
            contenido += '0' + (i % 10);
        }
    }
    
    int tamano_contenido = contenido.length();
    
    // Buscar inodo libre
    int nuevo_inodo_pos = obtenerInodoLibre(m.path_disco, sb.s_bm_inode_start, sb.s_inodes_count);
    if (nuevo_inodo_pos == -1) {
        return "ERROR: No hay inodos libres";
    }
    
    // Crear nuevo inodo
    Inodo nuevo_inodo;
    nuevo_inodo.i_uid = sesion_actual.uid;
    nuevo_inodo.i_gid = sesion_actual.gid;
    nuevo_inodo.i_size = tamano_contenido;
    nuevo_inodo.i_atime = time(nullptr);
    nuevo_inodo.i_ctime = time(nullptr);
    nuevo_inodo.i_mtime = time(nullptr);
    nuevo_inodo.i_type = 1;  // Archivo
    nuevo_inodo.i_perm[0] = '6';  // 6 = rw-
    nuevo_inodo.i_perm[1] = '6';  // 6 = rw-
    nuevo_inodo.i_perm[2] = '4';  // 4 = r--
    
    // Calcular bloques necesarios
    int bloques_necesarios = (tamano_contenido + 63) / 64;  // Redondear hacia arriba
    
    // Asignar bloques directos
    int bloque_actual = 0;
    int pos = 0;
    
    for (int i = 0; i < min(bloques_necesarios, 12); i++) {
        int bloque_libre = obtenerBloqueLibre(m.path_disco, sb.s_bm_block_start, sb.s_blocks_count);
        if (bloque_libre == -1) {
            return "ERROR: No hay bloques libres";
        }
        
        nuevo_inodo.i_block[i] = bloque_libre;
        marcarBitBloque(m.path_disco, sb.s_bm_block_start, sb.s_blocks_count, bloque_libre);
        
        // Escribir bloque
        BloqueArchivo bloque;
        memset(bloque.b_content, 0, 64);
        int copiar = min(64, tamano_contenido - pos);
        strncpy(bloque.b_content, contenido.substr(pos, copiar).c_str(), copiar);
        
        if (!escribirBloqueArchivo(m.path_disco, sb.s_block_start, bloque_libre, bloque)) {
            return "ERROR: No se pudo escribir bloque";
        }
        
        pos += copiar;
        bloque_actual++;
    }
    
    // Implementar bloques indirectos para archivos grandes
    
    // Marcar inodo como ocupado y escribirlo
    marcarBitInodo(m.path_disco, sb.s_bm_inode_start, sb.s_inodes_count, nuevo_inodo_pos);
    if (!escribirInodo(m.path_disco, sb.s_inode_start, nuevo_inodo_pos, nuevo_inodo)) {
        return "ERROR: No se pudo escribir inodo";
    }
    
    // Entrada en carpeta padre
    Inodo inodo_padre_obj;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_padre, inodo_padre_obj)) {
        return "ERROR: No se pudo leer inodo padre";
    }
    
    // Buscar un bloque de la carpeta padre con espacio
    bool entrada_agregada = false;
    for (int i = 0; i < 12 && inodo_padre_obj.i_block[i] != -1; i++) {
        BloqueCarpeta bloque_carpeta;
        if (!leerBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque_carpeta)) {
            continue;
        }
        
        // Buscar entrada libre
        for (int j = 0; j < 4; j++) {
            if (bloque_carpeta.b_content[j].b_inodo == -1) {
                // Espacio libre encontrado
                strcpy(bloque_carpeta.b_content[j].b_name, nombre_archivo.c_str());
                bloque_carpeta.b_content[j].b_inodo = nuevo_inodo_pos;
                
                if (!escribirBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque_carpeta)) {
                    return "ERROR: No se pudo actualizar bloque de carpeta";
                }
                entrada_agregada = true;
                break;
            }
        }
        if (entrada_agregada) break;
    }
    
    // Si no hay espacio en bloques existentes, asignar nuevo bloque de carpeta
    if (!entrada_agregada) {
        int nuevo_bloque_carpeta = obtenerBloqueLibre(m.path_disco, sb.s_bm_block_start, sb.s_blocks_count);
        if (nuevo_bloque_carpeta == -1) {
            return "ERROR: No hay bloques para expandir carpeta";
        }
        
        // Asignar bloque a la carpeta padre
        for (int i = 0; i < 12; i++) {
            if (inodo_padre_obj.i_block[i] == -1) {
                inodo_padre_obj.i_block[i] = nuevo_bloque_carpeta;
                marcarBitBloque(m.path_disco, sb.s_bm_block_start, sb.s_blocks_count, nuevo_bloque_carpeta);
                break;
            }
        }
        
        // Crear nuevo bloque con la entrada
        BloqueCarpeta nuevo_bloque;
        memset(&nuevo_bloque, 0, sizeof(BloqueCarpeta));
        for (int j = 0; j < 4; j++) {
            nuevo_bloque.b_content[j].b_inodo = -1;
        }
        strcpy(nuevo_bloque.b_content[0].b_name, nombre_archivo.c_str());
        nuevo_bloque.b_content[0].b_inodo = nuevo_inodo_pos;
        
        if (!escribirBloqueCarpeta(m.path_disco, sb.s_block_start, nuevo_bloque_carpeta, nuevo_bloque)) {
            return "ERROR: No se pudo escribir nuevo bloque de carpeta";
        }
        
        // Actualizar inodo padre
        if (!escribirInodo(m.path_disco, sb.s_inode_start, inodo_padre, inodo_padre_obj)) {
            return "ERROR: No se pudo actualizar inodo padre";
        }
    }
    
    // Registrar en journal (si es EXT3)
    registrarEnJournal(m.path_disco, m.part_start, "CREATE", path, contenido);

    return "MKFILE: Archivo '" + path + "' creado (" + to_string(tamano_contenido) + " bytes)";
}

// --- FUNCION: MKDIR (crear carpeta) ---
string mkdir(string path, bool p) {
    // Verificar sesion activa
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    // Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Separar ruta
    auto [ruta_padre, nombre_carpeta] = obtenerPadreYNombre(path);
    
    // Verificar permisos de escritura en carpeta padre
    
    // Si tiene -p, crear carpetas padre
    if (p) {
        if (!crearCarpetasPadre(m.path_disco, sb, ruta_padre, sesion_actual.uid, sesion_actual.gid)) {
            return "ERROR: No se pudieron crear las carpetas padre";
        }
    }
    
    // Crear la carpeta
    string resultado = mkdirInterno(m.path_disco, sb, path, sesion_actual.uid, sesion_actual.gid);
    
    if (resultado == "OK") {
        // Registrar en journal (si es EXT3)
        registrarEnJournal(m.path_disco, m.part_start, "MKDIR", path, "");

        return "MKDIR: Carpeta '" + path + "' creada exitosamente";
    } else {
        return resultado;
    }
}

// --- FUNCION: CAT (mostrar contenido de archivos) ---
string cat(vector<string> archivos) {
    // Verificar sesion activa
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    // Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Leer cada archivo
    string resultado = "";
    
    for (const string& archivo : archivos) {
        // Limpiar la ruta (quitar comillas si hay)
        string ruta = archivo;
        if (ruta.front() == '"' && ruta.back() == '"') {
            ruta = ruta.substr(1, ruta.length() - 2);
        }
        
        // Leer contenido
        string contenido = leerArchivoCompleto(m.path_disco, sb, ruta);
        
        if (contenido.empty()) {
            resultado += "ERROR: No se pudo leer el archivo '" + ruta + "' (no existe o no es archivo)\n\n";
        } else {
            resultado += contenido + "\n\n";
        }
    }
    
    return resultado;
}

// --- FUNCION: remove (eliminar archivo o carpeta) ---
string removeItem(string path) {
    // Verificar sesion activa
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    // Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Buscar inodo del archivo/carpeta
    int inodo_eliminar = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path);
    if (inodo_eliminar == -1) {
        return "ERROR: No existe la ruta: " + path;
    }
    
    // Obtener padre y nombre
    auto [ruta_padre, nombre] = obtenerPadreYNombre(path);
    int inodo_padre = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre);
    
    if (inodo_padre == -1) {
        return "ERROR: No existe la carpeta padre";
    }
    
    // Verificar permisos de escritura en el archivo/carpeta
    Inodo inodo;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_eliminar, inodo)) {
        return "ERROR: No se pudo leer el inodo";
    }
    
    // Root tiene todos los permisos
    bool tiene_permiso = (sesion_actual.usuario == "root");
    
    if (!tiene_permiso) {
        // Verificar si es propietario
        if (inodo.i_uid == sesion_actual.uid) {
            // Permiso de escritura para owner (bit 1 de 664 = 6 = rw-)
            tiene_permiso = (inodo.i_perm[0] == '6' || inodo.i_perm[0] == '7');
        }
    }
    
    if (!tiene_permiso) {
        return "ERROR: No tiene permisos de escritura para eliminar: " + path;
    }
    
    // ELiminar archivos y carpetas vacias
    
    if (inodo.i_type == 0) {
        // Si es carpeta verificar si tiene contenido
        bool tiene_contenido = false;
        for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
            BloqueCarpeta bloque;
            if (leerBloqueCarpeta(m.path_disco, sb.s_block_start, inodo.i_block[i], bloque)) {
                for (int j = 0; j < 4; j++) {
                    if (bloque.b_content[j].b_inodo != -1) {
                        string nombre_hijo(bloque.b_content[j].b_name);
                        if (nombre_hijo != "." && nombre_hijo != "..") {
                            tiene_contenido = true;
                            break;
                        }
                    }
                }
            }
            if (tiene_contenido) break;
        }
        
        if (tiene_contenido) {
            return "ERROR: La carpeta no esta vacia. Elimine su contenido primero";
        }
    }
    
    // Liberar los bloques del inodo
    for (int i = 0; i < 15 && inodo.i_block[i] != -1; i++) {
        liberarBitBloque(m.path_disco, sb.s_bm_block_start, sb.s_blocks_count, inodo.i_block[i]);
        inodo.i_block[i] = -1;
    }
    
    // Liberar el inodo
    liberarBitInodo(m.path_disco, sb.s_bm_inode_start, sb.s_inodes_count, inodo_eliminar);
    
    // Eliminar la entrada de la carpeta padre
    Inodo inodo_padre_obj;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_padre, inodo_padre_obj)) {
        return "ERROR: No se pudo leer inodo padre";
    }
    
    for (int i = 0; i < 12 && inodo_padre_obj.i_block[i] != -1; i++) {
        BloqueCarpeta bloque;
        if (leerBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque)) {
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo == inodo_eliminar) {
                    bloque.b_content[j].b_inodo = -1;
                    memset(bloque.b_content[j].b_name, 0, 12);
                    escribirBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque);
                    break;
                }
            }
        }
    }
    
    // Registrar en journal (si es EXT3)
    registrarEnJournal(m.path_disco, m.part_start, "REMOVE", path, "");
    
    return "REMOVE: '" + path + "' eliminado exitosamente";
}

// --- FUNCION: copy (copiar archivo o carpeta) ---
string copyItem(string path_origen, string path_destino) {
    // Verificar sesion activa
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    // Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Verificar que existe el origen
    int inodo_origen = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path_origen);
    if (inodo_origen == -1) {
        return "ERROR: No existe la ruta de origen: " + path_origen;
    }
    
    // Leer inodo del origen
    Inodo inodo_orig;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_origen, inodo_orig)) {
        return "ERROR: No se pudo leer el inodo de origen";
    }
    
    // Verificar permisos de lectura en origen
    bool puede_leer = (sesion_actual.usuario == "root");
    if (!puede_leer && inodo_orig.i_uid == sesion_actual.uid) {
        // Permiso de lectura para owner (primer digito 4,5,6,7 tiene lectura)
        puede_leer = (inodo_orig.i_perm[0] == '4' || inodo_orig.i_perm[0] == '5' || inodo_orig.i_perm[0] == '6' || inodo_orig.i_perm[0] == '7');
    }
    
    if (!puede_leer) {
        return "ERROR: No tiene permisos de lectura para copiar: " + path_origen;
    }
    
    // Construir ruta completa de destino (origen puede ser archivo/carpeta)
    auto [ruta_destino_padre, nombre_destino] = obtenerPadreYNombre(path_destino);
    
    // Si el destino es solo una carpeta, mantener el nombre original
    string nombre_final = nombre_destino;
    if (nombre_destino.empty()) {
        auto [_, nombre_orig] = obtenerPadreYNombre(path_origen);
        nombre_final = nombre_orig;
        path_destino = ruta_destino_padre + "/" + nombre_orig;
    }
    
    // Verificar que no exista ya en destino
    int inodo_destino = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path_destino);
    if (inodo_destino != -1) {
        return "ERROR: Ya existe un archivo/carpeta en destino: " + path_destino;
    }
    
    // Verificar que la carpeta destino padre exista
    int inodo_destino_padre = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_destino_padre);
    if (inodo_destino_padre == -1) {
        return "ERROR: La carpeta destino padre no existe: " + ruta_destino_padre;
    }
    
    // Verificar permisos de escritura en destino padre
    Inodo inodo_dest_padre;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_destino_padre, inodo_dest_padre)) {
        return "ERROR: No se pudo leer inodo de destino padre";
    }
    
    bool puede_escribir = (sesion_actual.usuario == "root");
    if (!puede_escribir && inodo_dest_padre.i_uid == sesion_actual.uid) {
        puede_escribir = (inodo_dest_padre.i_perm[0] == '6' || inodo_dest_padre.i_perm[0] == '7');
    }
    
    if (!puede_escribir) {
        return "ERROR: No tiene permisos de escritura en la carpeta destino: " + ruta_destino_padre;
    }
    
    // Copiar segun tipo (archivo o carpeta)
    if (inodo_orig.i_type == 1) {
        // Es archivo: copiar contenido
        string contenido = leerArchivoCompleto(m.path_disco, sb, path_origen);
        
        // Crear nuevo archivo en destino
        string resultado = mkfileInterno(m.path_disco, sb, path_destino, contenido, sesion_actual.uid, sesion_actual.gid);
        if (resultado != "OK") {
            return resultado;
        }
    } else {
        // Carpeta: crear carpeta vacia en destino
        string resultado = mkdirInterno(m.path_disco, sb, path_destino, sesion_actual.uid, sesion_actual.gid);
        if (resultado != "OK") {
            return resultado;
        }
        
        // Copiar contenido recursivamente para carpetas
    }
    
    registrarEnJournal(m.path_disco, m.part_start, "COPY", path_origen + " -> " + path_destino, "");
    
    return "COPY: '" + path_origen + "' copiado a '" + path_destino + "' exitosamente";
}

// --- FUNCION: move (mover archivo o carpeta) ---
string moveItem(string path_origen, string path_destino) {
    // Verificar sesion activa
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    // Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Verificar que existe el origen
    int inodo_origen = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path_origen);
    if (inodo_origen == -1) {
        return "ERROR: No existe la ruta de origen: " + path_origen;
    }
    
    // Obtener padre y nombre del origen
    auto [ruta_padre_origen, nombre_origen] = obtenerPadreYNombre(path_origen);
    
    // Construir ruta destino completa
    auto [ruta_padre_destino, nombre_destino] = obtenerPadreYNombre(path_destino);
    string nombre_final = nombre_destino.empty() ? nombre_origen : nombre_destino;
    string ruta_destino_completa = ruta_padre_destino + "/" + nombre_final;
    
    // Verificar que no exista ya en destino
    int inodo_destino = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_destino_completa);
    if (inodo_destino != -1) {
        return "ERROR: Ya existe un archivo/carpeta en destino: " + ruta_destino_completa;
    }
    
    // Verificar permisos de escritura en origen, para mover
    Inodo inodo_orig;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_origen, inodo_orig)) {
        return "ERROR: No se pudo leer el inodo de origen";
    }
    
    bool puede_mover = (sesion_actual.usuario == "root");
    if (!puede_mover && inodo_orig.i_uid == sesion_actual.uid) {
        puede_mover = (inodo_orig.i_perm[0] == '6' || inodo_orig.i_perm[0] == '7');
    }
    
    if (!puede_mover) {
        return "ERROR: No tiene permisos para mover: " + path_origen;
    }
    
    // Verificar que la carpeta destino padre exista
    int inodo_destino_padre = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre_destino);
    if (inodo_destino_padre == -1) {
        return "ERROR: La carpeta destino padre no existe: " + ruta_padre_destino;
    }
    
    // Verificar permisos de escritura en destino padre
    Inodo inodo_dest_padre;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_destino_padre, inodo_dest_padre)) {
        return "ERROR: No se pudo leer inodo de destino padre";
    }
    
    bool puede_escribir = (sesion_actual.usuario == "root");
    if (!puede_escribir && inodo_dest_padre.i_uid == sesion_actual.uid) {
        puede_escribir = (inodo_dest_padre.i_perm[0] == '6' || inodo_dest_padre.i_perm[0] == '7');
    }
    
    if (!puede_escribir) {
        return "ERROR: No tiene permisos de escritura en la carpeta destino: " + ruta_padre_destino;
    }
    
    // Agregar entrada en carpeta destino
    Inodo inodo_dest_padre_obj;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_destino_padre, inodo_dest_padre_obj)) {
        return "ERROR: No se pudo leer inodo destino padre";
    }
    
    bool entrada_agregada = false;
    for (int i = 0; i < 12 && inodo_dest_padre_obj.i_block[i] != -1; i++) {
        BloqueCarpeta bloque;
        if (leerBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_dest_padre_obj.i_block[i], bloque)) {
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo == -1) {
                    strcpy(bloque.b_content[j].b_name, nombre_final.c_str());
                    bloque.b_content[j].b_inodo = inodo_origen;
                    escribirBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_dest_padre_obj.i_block[i], bloque);
                    entrada_agregada = true;
                    break;
                }
            }
        }
        if (entrada_agregada) break;
    }
    
    if (!entrada_agregada) {
        return "ERROR: No hay espacio en carpeta destino";
    }
    
    // Eliminar entrada de carpeta origen
    int inodo_padre_origen_int = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre_origen);
    if (inodo_padre_origen_int != -1) {
        Inodo inodo_padre_origen_obj;
        if (leerInodo(m.path_disco, sb.s_inode_start, inodo_padre_origen_int, inodo_padre_origen_obj)) {
            for (int i = 0; i < 12 && inodo_padre_origen_obj.i_block[i] != -1; i++) {
                BloqueCarpeta bloque;
                if (leerBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_origen_obj.i_block[i], bloque)) {
                    for (int j = 0; j < 4; j++) {
                        if (bloque.b_content[j].b_inodo == inodo_origen) {
                            bloque.b_content[j].b_inodo = -1;
                            memset(bloque.b_content[j].b_name, 0, 12);
                            escribirBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_origen_obj.i_block[i], bloque);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    registrarEnJournal(m.path_disco, m.part_start, "MOVE", path_origen + " -> " + ruta_destino_completa, "");
    
    return "MOVE: '" + path_origen + "' movido a '" + ruta_destino_completa + "' exitosamente";
}

// --- FUNCION: rename (cambiar nombre de archivo o carpeta) ---
string renameItem(string path, string nuevo_nombre) {
    // Verificar sesion activa
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    // Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Verificar que existe el archivo/carpeta
    int inodo_renombrar = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path);
    if (inodo_renombrar == -1) {
        return "ERROR: No existe la ruta: " + path;
    }
    cout << "DEBUG: inodo a renombrar = " << inodo_renombrar << endl;
    
    // Obtener padre y nombre actual
    auto [ruta_padre, nombre_actual] = obtenerPadreYNombre(path);
    int inodo_padre = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_padre);
    
    if (inodo_padre == -1) {
        return "ERROR: No existe la carpeta padre";
    }
    cout << "DEBUG: inodo padre = " << inodo_padre << endl;
    cout << "DEBUG: nombre actual = " << nombre_actual << endl;
    
    // Construir nueva ruta para verificar que no exista
    string ruta_nueva = (ruta_padre == "/" ? "/" + nuevo_nombre : ruta_padre + "/" + nuevo_nombre);
    int inodo_existente = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta_nueva);
    if (inodo_existente != -1) {
        return "ERROR: Ya existe un archivo/carpeta con nombre '" + nuevo_nombre + "' en esta ubicacion";
    }
    
    // Verificar permisos de escritura
    Inodo inodo;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_renombrar, inodo)) {
        return "ERROR: No se pudo leer el inodo";
    }
    
    bool tiene_permiso = (sesion_actual.usuario == "root");
    if (!tiene_permiso && inodo.i_uid == sesion_actual.uid) {
        tiene_permiso = (inodo.i_perm[0] == '6' || inodo.i_perm[0] == '7');
    }
    
    if (!tiene_permiso) {
        return "ERROR: No tiene permisos de escritura para renombrar: " + path;
    }
    
    // Leer el inodo de la carpeta padre
    Inodo inodo_padre_obj;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_padre, inodo_padre_obj)) {
        return "ERROR: No se pudo leer inodo padre";
    }
    
    // Buscar la entrada en la carpeta padre y cambiar el nombre
    bool encontrado = false;
    for (int i = 0; i < 12 && inodo_padre_obj.i_block[i] != -1; i++) {
        BloqueCarpeta bloque;
        if (leerBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque)) {
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo == inodo_renombrar) {
                    cout << "DEBUG: Encontrado en bloque " << i << ", posicion " << j << endl;
                    cout << "DEBUG: Nombre actual en bloque: " << bloque.b_content[j].b_name << endl;
                    
                    // Limpiar el nombre actual
                    memset(bloque.b_content[j].b_name, 0, 12);
                    // Copiar el nuevo nombre
                    strcpy(bloque.b_content[j].b_name, nuevo_nombre.c_str());
                    
                    cout << "DEBUG: Nuevo nombre escrito: " << bloque.b_content[j].b_name << endl;
                    
                    // Escribir el bloque actualizado
                    if (!escribirBloqueCarpeta(m.path_disco, sb.s_block_start, inodo_padre_obj.i_block[i], bloque)) {
                        return "ERROR: No se pudo escribir el bloque actualizado";
                    }
                    cout << "DEBUG: Bloque escrito correctamente" << endl;
                    encontrado = true;
                    break;
                }
            }
        }
        if (encontrado) break;
    }
    
    if (!encontrado) {
        return "ERROR: No se encontro la entrada en la carpeta padre";
    }
    
    // Registrar en journal (si es EXT3)
    registrarEnJournal(m.path_disco, m.part_start, "RENAME", path + " -> " + nuevo_nombre, "");
    
    return "RENAME: '" + path + "' renombrado a '" + nuevo_nombre + "' exitosamente";
}

// --- FUNCION: find (buscar archivos por nombre) ---
string find(string path, string name) {
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    stringstream resultado;
    resultado << "=== BUSQUEDA: " << name << " en " << path << " ===\n";
    
    // Función recursiva para buscar
    function<void(int, string)> buscar = [&](int inodo_actual, string ruta_actual) {
        Inodo inodo;
        if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_actual, inodo)) return;
        
        if (inodo.i_type != 0) return; // Solo carpetas
        
        for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
            BloqueCarpeta bloque;
            if (!leerBloqueCarpeta(m.path_disco, sb.s_block_start, inodo.i_block[i], bloque)) continue;
            
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo != -1) {
                    string nombre(bloque.b_content[j].b_name);
                    if (nombre == "." || nombre == "..") continue;
                    
                    string ruta_completa = (ruta_actual == "/" ? "/" + nombre : ruta_actual + "/" + nombre);
                    
                    // Comparar con comodines
                    bool coincide = false;
                    if (name.find('*') != string::npos) {
                        string patron = name;
                        patron.erase(remove(patron.begin(), patron.end(), '*'), patron.end());
                        coincide = (nombre.find(patron) != string::npos);
                    } else if (name.find('?') != string::npos) {
                        coincide = (nombre.length() == name.length());
                    } else {
                        coincide = (nombre == name);
                    }
                    
                    if (coincide) {
                        Inodo inodo_hijo;
                        if (leerInodo(m.path_disco, sb.s_inode_start, bloque.b_content[j].b_inodo, inodo_hijo)) {
                            resultado << ruta_completa << " (" << (inodo_hijo.i_type == 0 ? "DIR" : "FILE") << ")\n";
                        }
                    }
                    
                    buscar(bloque.b_content[j].b_inodo, ruta_completa);
                }
            }
        }
    };
    
    int inodo_inicio = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path);
    if (inodo_inicio == -1) return "ERROR: Ruta no existe: " + path;
    
    buscar(inodo_inicio, path);
    
    return resultado.str();
}

// --- FUNCION: chown (cambiar propietario) ---
string chown(string path, string usuario, bool recursivo) {
    if (!sesion_actual.activa) {
        return "ERROR: No hay sesion activa";
    }
    
    if (sesion_actual.usuario != "root") {
        return "ERROR: Solo root puede cambiar el propietario";
    }
    
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == sesion_actual.id_particion) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Leer users.txt para obtener el UID del usuario
    string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, 
                               sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
    
    int nuevo_uid = -1;
    stringstream ss(users);
    string linea;
    while (getline(ss, linea)) {
        vector<string> partes;
        stringstream ss2(linea);
        string parte;
        while (getline(ss2, parte, ',')) {
            partes.push_back(parte);
        }
        if (partes.size() >= 5 && partes[1] == "U" && partes[3] == usuario) {
            nuevo_uid = stoi(partes[0]);
            break;
        }
    }
    
    if (nuevo_uid == -1) {
        return "ERROR: Usuario '" + usuario + "' no existe";
    }
    
    int inodo = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, path);
    if (inodo == -1) return "ERROR: Ruta no existe: " + path;
    
    Inodo inodo_obj;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo, inodo_obj)) {
        return "ERROR: No se pudo leer inodo";
    }
    
    inodo_obj.i_uid = nuevo_uid;
    
    if (!escribirInodo(m.path_disco, sb.s_inode_start, inodo, inodo_obj)) {
        return "ERROR: No se pudo escribir inodo";
    }
    
    // Si es recursivo, hay que implementarlo (opcional)
    
    return "CHOWN: Propietario de '" + path + "' cambiado a '" + usuario + "'";
}

// ******** FUNCIONES DE REPORTES ********

// GENERAR REPORTE MBR
string generarReporteMBR(string path_disco, string path_jpg) {
    MBR mbr;
    if (!leerMBR(path_disco, mbr)) {
        return "ERROR: No se pudo leer el MBR";
    }

    string path_dot = path_jpg + ".tmp.dot";
    ofstream dot(path_dot.c_str());
    
    dot << "digraph G {" << endl;
    dot << "  node [shape=record];" << endl;
    dot << "  mbr [label=\"{MBR|Tamaño: " << mbr.mbr_size << " bytes|";
    
    char fecha[20];
    struct tm *tm_info = localtime(&mbr.mbr_creation_date);
    strftime(fecha, 20, "%d/%m/%Y %H:%M", tm_info);
    dot << "Fecha: " << fecha << "|";
    dot << "Signature: " << mbr.mbr_dsk_signature;
    
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            dot << "|{Part" << i+1 << "|" << mbr.mbr_partitions[i].part_name << "}";
        }
    }
    
    dot << "}\"];" << endl;
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_jpg + "\" 2>/dev/null";
    system(comando.c_str());
    remove(path_dot.c_str());
    
    return "Reporte MBR generado en " + path_jpg;
}

// GENERAR REPORYE DISK
string generarReporteDISK(string path_disco, string path_jpg) {
    MBR mbr;
    if (!leerMBR(path_disco, mbr)) {
        return "ERROR: No se pudo leer el MBR";
    }
    
    string path_dot = path_jpg + ".tmp.dot";
    ofstream dot(path_dot.c_str());

    dot << "digraph G {" << endl;
    dot << "  node [shape=record];" << endl;
    dot << "  disk [label=\"";
    
    int total = mbr.mbr_size;
    int inicio = sizeof(MBR);
    
    vector<Partition> particiones;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            particiones.push_back(mbr.mbr_partitions[i]);
        }
    }

    for (int i = 0; i < particiones.size(); i++) {
        for (int j = i + 1; j < particiones.size(); j++) {
            if (particiones[i].part_start > particiones[j].part_start) {
                swap(particiones[i], particiones[j]);
            }
        }
    }

    if (!particiones.empty() && particiones[0].part_start > inicio) {
        int libre = particiones[0].part_start - inicio;
        dot << "MBR|Libre " << (libre * 100 / total) << "%|";
    } else {
        dot << "MBR|";
    }
    
    for (int i = 0; i < particiones.size(); i++) {
        dot << particiones[i].part_name << " " << (particiones[i].part_size * 100 / total) << "%";
        if (i < particiones.size() - 1) dot << "|";
    }
    
    if (!particiones.empty()) {
        int ultimo_fin = particiones.back().part_start + particiones.back().part_size;
        if (ultimo_fin < total) {
            int libre = total - ultimo_fin;
            dot << "|Libre " << (libre * 100 / total) << "%";
        }
    }
    
    dot << "\"];" << endl;
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_jpg + "\" 2>/dev/null";
    system(comando.c_str());
    remove(path_dot.c_str());
    
    return "Reporte DISK generado en " + path_jpg;
}

// GENERAR REPORTE EBR
string generarReporteEBR(string path_disco, string path_jpg, int start) {
    string path_dot = path_jpg + ".tmp.dot";
    ofstream dot(path_dot.c_str());
    
    dot << "digraph G {" << endl;
    dot << "  node [shape=record];" << endl;
    dot << "  rankdir=LR;" << endl;
    
    int ebr_pos = start;
    EBR ebr;
    int contador = 0;
    
    while (ebr_pos != -1) {
        if (!leerEBR(path_disco, ebr_pos, ebr)) break;
        
        dot << "  ebr" << contador << " [label=\"{EBR " << contador << "|";
        dot << "Mount: " << ebr.part_mount << "|";
        dot << "Fit: " << ebr.part_fit << "|";
        dot << "Start: " << ebr.part_start << "|";
        dot << "Size: " << ebr.part_size << "|";
        dot << "Next: " << ebr.part_next << "|";
        dot << "Name: " << ebr.part_name << "}\"];" << endl;
        
        if (ebr.part_next != -1) {
            dot << "  ebr" << contador << " -> ebr" << (contador+1) << ";" << endl;
        }
        
        ebr_pos = ebr.part_next;
        contador++;
    }
    
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_jpg + "\" 2>/dev/null";
    system(comando.c_str());
    remove(path_dot.c_str());
    
    return "Reporte EBR generado en " + path_jpg;
}

// GENERAR REPORTE INODE (inodos usados)
string generarReporteInode(string path_disco, Superblock &sb, string path_salida) {
    string path_dot = path_salida + ".tmp.dot";
    ofstream dot(path_dot.c_str());
    
    dot << "digraph G {" << endl;
    dot << "  node [shape=record];" << endl;
    dot << "  rankdir=LR;" << endl;
    
    for (int i = 0; i < sb.s_inodes_count; i++) {
        // Leer bitmap para ver si esta usado
        ifstream disco(path_disco, ios::binary);
        disco.seekg(sb.s_bm_inode_start + i);
        char bit;
        disco.read(&bit, 1);
        disco.close();
        
        if (bit == 1) {
            Inodo inodo;
            if (!leerInodo(path_disco, sb.s_inode_start, i, inodo)) continue;
            
            // Nodo del INODO
            dot << "  inodo" << i << " [label=\"{Inodo " << i << "|";
            dot << "UID: " << inodo.i_uid << "|";
            dot << "GID: " << inodo.i_gid << "|";
            dot << "Size: " << inodo.i_size << "|";
            dot << "Tipo: " << (inodo.i_type == 0 ? "Carpeta" : "Archivo") << "|";
            dot << "Perm: " << inodo.i_perm[0] << inodo.i_perm[1] << inodo.i_perm[2] << "|";
            dot << "{\\l";
            
            // Bloques directos
            for (int j = 0; j < 12; j++) {
                if (inodo.i_block[j] != -1) {
                    dot << "Bloque directo " << j << ": " << inodo.i_block[j] << "\\l";
                }
            }
            
            // Bloques indirectos
            if (inodo.i_block[12] != -1) {
                dot << "Simple indirecto: " << inodo.i_block[12] << "\\l";
            }
            if (inodo.i_block[13] != -1) {
                dot << "Doble indirecto: " << inodo.i_block[13] << "\\l";
            }
            if (inodo.i_block[14] != -1) {
                dot << "Triple indirecto: " << inodo.i_block[14] << "\\l";
            }
            
            dot << "}}\"];" << endl;
            
            // Conectar con bloques directos
            for (int j = 0; j < 12; j++) {
                if (inodo.i_block[j] != -1) {
                    dot << "  inodo" << i << " -> bloque" << inodo.i_block[j] << ";" << endl;
                }
            }
            
            // Conectar con bloques indirectos
            if (inodo.i_block[12] != -1) {
                dot << "  inodo" << i << " -> bloque" << inodo.i_block[12] << " [label=\"simple\"];" << endl;
            }
            if (inodo.i_block[13] != -1) {
                dot << "  inodo" << i << " -> bloque" << inodo.i_block[13] << " [label=\"doble\"];" << endl;
            }
            if (inodo.i_block[14] != -1) {
                dot << "  inodo" << i << " -> bloque" << inodo.i_block[14] << " [label=\"triple\"];" << endl;
            }
        }
    }
    
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_salida + "\"";
    system(comando.c_str());
    remove(path_dot.c_str());
    
    return "Reporte INODE generado en " + path_salida;
}

// GENERAR REPORTE BLOCK (bloques usados)
string generarReporteBlock(string path_disco, Superblock &sb, string path_salida) {
    cout << "DEBUG: Generando block en: " << path_salida << endl;
    
    string path_dot = path_salida + ".tmp.dot";
    ofstream dot(path_dot.c_str());
    
    dot << "digraph G {" << endl;
    dot << "  node [shape=record];" << endl;
    dot << "  rankdir=LR;" << endl;
    
    for (int i = 0; i < sb.s_blocks_count && i < 50; i++) { // Solo primeros 50 para no saturar
        ifstream disco(path_disco, ios::binary);
        disco.seekg(sb.s_bm_block_start + i);
        char bit;
        disco.read(&bit, 1);
        disco.close();
        
        if (bit == 1) {
            dot << "  bloque" << i << " [label=\"{Bloque " << i << "|OCUPADO}\"];" << endl;
        }
    }
    
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_salida + "\" 2>&1";
    cout << "EJECUTANDO: " << comando << endl;
    system(comando.c_str());
    
    // Verificar si se creó
    ifstream test(path_salida.c_str());
    if (test.good()) {
        cout << "Archivo creado: " << path_salida << endl;
        test.close();
    } else {
        cout << "ERROR: No se creó el archivo" << endl;
    }
    
    remove(path_dot.c_str());
    
    return "Reporte BLOCK generado en " + path_salida;
}

// GENERAR REPORTE BM_INODE (bitmap de inodos)
string generarReporteBMInode(string path_disco, Superblock &sb, string path_salida) {
    ofstream txt(path_salida.c_str());
    
    ifstream disco(path_disco, ios::binary);
    disco.seekg(sb.s_bm_inode_start);
    
    int total_bits = sb.s_inodes_count;
    char* bitmap = new char[total_bits];
    disco.read(bitmap, total_bits);
    disco.close();
    
    for (int i = 0; i < total_bits; i++) {
        txt << (bitmap[i] == 1 ? '1' : '0');
        if ((i + 1) % 20 == 0) txt << endl;
    }
    
    delete[] bitmap;
    txt.close();
    
    return "Reporte BM_INODE generado en " + path_salida;
}

// GENERAR REPORTE BM_BLOCK (bitmap de bloques)
string generarReporteBMBlock(string path_disco, Superblock &sb, string path_salida) {
    ofstream txt(path_salida.c_str());
    
    ifstream disco(path_disco, ios::binary);
    disco.seekg(sb.s_bm_block_start);
    
    int total_bits = sb.s_blocks_count;
    char* bitmap = new char[total_bits];
    disco.read(bitmap, total_bits);
    disco.close();
    
    for (int i = 0; i < total_bits; i++) {
        txt << (bitmap[i] == 1 ? '1' : '0');
        if ((i + 1) % 20 == 0) txt << endl;
    }
    
    delete[] bitmap;
    txt.close();
    
    return "Reporte BM_BLOCK generado en " + path_salida;
}

// GENERAR REPORTE SB (SUPERBLOQUE)
string generarReporteSB(string path_disco, Superblock &sb, string path_salida) {
    string path_dot = path_salida + ".tmp.dot";
    ofstream dot(path_dot.c_str());
    
    dot << "digraph G {" << endl;
    dot << "  node [shape=record];" << endl;
    dot << "  sb [label=\"{SUPERBLOQUE|";
    dot << "Inodos: " << sb.s_inodes_count << "|";
    dot << "Bloques: " << sb.s_blocks_count << "|";
    dot << "Libres inodos: " << sb.s_free_inodes_count << "|";
    dot << "Libres bloques: " << sb.s_free_blocks_count << "|";
    dot << "Magic: 0x" << hex << sb.s_magic << dec;
    dot << "}\"];" << endl;
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_salida + "\" 2>/dev/null";
    system(comando.c_str());
    remove(path_dot.c_str());
    
    return "Reporte SB generado en " + path_salida;
}

// GENERAR REPORTE TREE (Arbol del sistema)
string generarReporteTree(string path_disco, Superblock &sb, string path_salida) {
    string path_dot = path_salida + ".tmp.dot";
    ofstream dot(path_dot.c_str());
    
    dot << "digraph G {" << endl;
    dot << "  node [shape=record];" << endl;
    dot << "  rankdir=TB;" << endl;
    
    int nodo_count = 0;
    generarTreeRecursivo(dot, path_disco, sb, 0, nodo_count);
    
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_salida + "\" 2>&1";
    system(comando.c_str());
    remove(path_dot.c_str());
    
    return "Reporte TREE generado en " + path_salida;
}

// GENERAR REPORTE FILE (Contenido de archivo)
string generarReporteFile(string path_disco, Superblock &sb, string path_salida, string path_file) {
    // Verificar que se especificó el archivo
    if (path_file.empty()) {
        return "ERROR: Falta especificar el archivo con -path_file_ls";
    }
    
    // Leer contenido del archivo usando la función que ya tienes
    string contenido = leerArchivoCompleto(path_disco, sb, path_file);
    
    if (contenido.empty()) {
        return "ERROR: No se pudo leer el archivo '" + path_file + "' (no existe o no es archivo)";
    }
    
    // Asegurar que la extensión sea .txt
    string path_real = path_salida;
    // Si termina en .jpg, cambiarlo a .txt
    if (path_real.length() > 4 && path_real.substr(path_real.length()-4) == ".jpg") {
        path_real = path_real.substr(0, path_real.length()-4) + ".txt";
    }
    // Si termina en .png, cambiarlo a .txt
    else if (path_real.length() > 4 && path_real.substr(path_real.length()-4) == ".png") {
        path_real = path_real.substr(0, path_real.length()-4) + ".txt";
    }
    
    // Guardar en archivo de texto
    ofstream txt(path_real.c_str());
    if (!txt.is_open()) {
        return "ERROR: No se pudo crear el archivo de salida: " + path_real;
    }
    
    txt << "=== CONTENIDO DE: " << path_file << " ===" << endl;
    txt << contenido << endl;
    txt.close();
    
    cout << "DEBUG: Archivo FILE generado en: " << path_real << endl;
    
    return "Reporte FILE generado en " + path_real;
}

// GENERAR REPORTE LS (listado detallado)
string generarReporteLS(string path_disco, Superblock &sb, string path_salida, string path_dir) {
    // Si no sse especifica el directorio, usar raiz
    if (path_dir.empty()) {
        path_dir = "/";
    }
    
    // Buscar inodo del directorio
    int inodo_dir = buscarInodoPorRuta(path_disco, sb.s_inode_start, sb.s_block_start, path_dir);
    if (inodo_dir == -1) {
        return "ERROR: No existe el directorio: " + path_dir;
    }
    
    // Leer inodo del directorio
    Inodo dir_inodo;
    if (!leerInodo(path_disco, sb.s_inode_start, inodo_dir, dir_inodo)) {
        return "ERROR: No se pudo leer el inodo del directorio";
    }
    
    // Verificar que sea directorio
    if (dir_inodo.i_type != 0) {
        return "ERROR: La ruta no es un directorio";
    }
    
    string path_dot = path_salida + ".tmp.dot";
    ofstream dot(path_dot.c_str());
    
    dot << "digraph G {" << endl;
    dot << "  node [shape=plaintext];" << endl;
    dot << "  ls [label=<" << endl;
    dot << "    <table border='1' cellborder='1' cellspacing='0'>" << endl;
    dot << "    <tr>" << endl;
    dot << "      <td><b>Permisos</b></td>" << endl;
    dot << "      <td><b>Owner</b></td>" << endl;
    dot << "      <td><b>Grupo</b></td>" << endl;
    dot << "      <td><b>Tamaño</b></td>" << endl;
    dot << "      <td><b>Fecha</b></td>" << endl;
    dot << "      <td><b>Tipo</b></td>" << endl;
    dot << "      <td><b>Nombre</b></td>" << endl;
    dot << "    </tr>" << endl;
    
    // Recorrer bloques del directorio
    for (int i = 0; i < 12 && dir_inodo.i_block[i] != -1; i++) {
        BloqueCarpeta bloque;
        if (!leerBloqueCarpeta(path_disco, sb.s_block_start, dir_inodo.i_block[i], bloque)) continue;
        
        for (int j = 0; j < 4; j++) {
            if (bloque.b_content[j].b_inodo != -1) {
                string nombre(bloque.b_content[j].b_name);
                if (nombre == "." || nombre == "..") continue;  // Ignorar . y ..
                
                int inodo_hijo = bloque.b_content[j].b_inodo;
                Inodo hijo;
                if (!leerInodo(path_disco, sb.s_inode_start, inodo_hijo, hijo)) continue;
                
                // Formatear fecha
                char fecha[20];
                struct tm *tm_info = localtime(&hijo.i_ctime);
                strftime(fecha, 20, "%d/%m/%Y %H:%M", tm_info);
                
                dot << "    <tr>" << endl;
                dot << "      <td>" << hijo.i_perm[0] << hijo.i_perm[1] << hijo.i_perm[2] << "</td>" << endl;
                dot << "      <td>" << hijo.i_uid << "</td>" << endl;
                dot << "      <td>" << hijo.i_gid << "</td>" << endl;
                dot << "      <td>" << hijo.i_size << "</td>" << endl;
                dot << "      <td>" << fecha << "</td>" << endl;
                dot << "      <td>" << (hijo.i_type == 0 ? "DIR" : "FILE") << "</td>" << endl;
                dot << "      <td>" << nombre << "</td>" << endl;
                dot << "    </tr>" << endl;
            }
        }
    }
    
    dot << "    </table>" << endl;
    dot << "  >];" << endl;
    dot << "}" << endl;
    dot.close();
    
    string comando = "dot -Tjpg \"" + path_dot + "\" -o \"" + path_salida + "\"";
    system(comando.c_str());
    remove(path_dot.c_str());
    
    return "Reporte LS generado en " + path_salida;
}


// --- FUNCION: rep (generar reportes) ---
string rep(string name, string path, string id, string path_file_ls) {

    // Crear carpetas si no existen
    size_t last_slash = path.find_last_of('/');
    if (last_slash != string::npos) {
        string folder = path.substr(0, last_slash);
        string comando_mkdir = "mkdir -p \"" + folder + "\"";
        system(comando_mkdir.c_str());
    }
    
    // Buscar el disco segun el ID
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return "ERROR: No existe particion montada con ID: " + id;
    }
    
    Montada& m = particiones_montadas[idx];

    // Leer superbloque de la particion
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque de la particion";
    }

    // Generar reporte segun el tipo
    string resultado;

    // Generar reporte MBR
    if (name == "mbr") {
        resultado = generarReporteMBR(m.path_disco, path);
    }
    // Generar reporte DISK
    else if (name == "disk") {
        resultado = generarReporteDISK(m.path_disco, path);
    }
    // Generar reporte EBR
    else if (name == "ebr") {
        resultado = generarReporteEBR(m.path_disco, path, 168);
    }
    // Generar reporte inode (inoos usados)
    else if (name == "inode") {
        resultado = generarReporteInode(m.path_disco, sb, path);
    }
    // Generar reporte block (bloques usados)
    else if (name == "block") {
        resultado = generarReporteBlock(m.path_disco, sb, path);
    }
    // Generar reporte bm_inode (mapa de bits de inodos)
    else if (name == "bm_inode") {
        resultado = generarReporteBMInode(m.path_disco, sb, path);
    }
    // Generar reporte bm_block (mapa de bits de bloques)
    else if (name == "bm_block") {
        resultado = generarReporteBMBlock(m.path_disco, sb, path);
    }
    // Generar reporte SB (superbloque)
    else if (name == "sb") {
        resultado = generarReporteSB(m.path_disco, sb, path);
    }
    // Generar reporte TREE (Arbol del sistema)
    else if (name == "tree") {
        resultado = generarReporteTree(m.path_disco, sb, path);
    }
    // Generar reporte FILE (contenido de archivo)
    else if (name == "file") {
        if (path_file_ls.empty()) {
            return "ERROR: Falta -path_file_ls para reporte FILE";
        }
        resultado = generarReporteFile(m.path_disco, sb, path, path_file_ls);
    }
    // Generar reporte LS (listado detallado)
    else if (name == "ls") {
        string dir = path_file_ls.empty() ? "/" : path_file_ls;
        resultado = generarReporteLS(m.path_disco, sb, path, dir);
    }
    else {
        return "ERROR: El reporte no existe: " + name;
    }

    // COnvertir a jpg
    string path_dot = path + ".dot";
    ifstream test_dot(path_dot.c_str());
    if (test_dot.good()) {
        test_dot.close();
        string comando_dot = "dot -Tjpg -o \"" + path + "\" \"" + path_dot + "\"";
        system(comando_dot.c_str());
        remove(path_dot.c_str());  // Borrar .dot temporal
    }

    // Abrir la imagen automaticamente
    string comando_abrir = "xdg-open \"" + path + "\"";
    system(comando_abrir.c_str());
    
    return resultado;
}

// --- FUNCION: procesar_comando ---
string procesar_comando(const string& comando) {

    // MKDISK: Crear disco
    if (comando.find("mkdisk") == 0) {

        int size = 0;
        string unit = "M";
        string fit = "FF";
        string path = "";
    
        // EXTRAER PARAMETRO: -size
        size_t pos = comando.find("-size=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            size = stoi(valor);  // Convertir string a entero
        }
        
        // EXTRAER PARAMETRO: -unit (opcional)
        pos = comando.find("-unit=");
        if (pos != string::npos) {
            unit = comando[pos + 6];  // Toma el caracter despues de "unit="
        }
        
        // EXTRAR PARAMETRO: -fit (opcional)
        pos = comando.find("-fit=");
        if (pos != string::npos) {
            fit = comando.substr(pos + 5, 2);  // Toma 2 caracteres (BF, FF, WF)
        }
        
        // EXTRAER PARAMETRO: -path
        pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
                
            // Verificar si la ruta esta entre comillas
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);  // Extraer sin comillas
            } else {
                // Si no tiene comillas se toma hasta el espacio
                path = valor.substr(0, valor.find(' '));
            }
        }
    
        // VALIDAR LOS PARAMETROS OBLIGATORIOS
        if (size <= 0) {
                return "EROR: Faltan parametros";
        }
        if (path.empty()) {
            return "ERROR: Falta el parametro -path";
        }
        
        // EJECUTAR mkdisk
        return mkdisk(size, unit, fit, path);
    }


    // RMDISK: Eliminar disco
    else if (comando.find("rmdisk") == 0) {

        string path = ""; // RUta del disco a eliminar

        // EXTRAER PARAMETRO: -path
        size_t pos = comando.find("-path");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);

            // Verificar si la ruta esta entre comillas
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);  // Extraer sin comillas
            } else {
                // Si no tiene comillas se toma hasta el espacio
                path = valor.substr(0, valor.find(' '));
            }
        }

        // VALIDAR PARAMETROS OBLIGATORIOS
        if (path.empty()) {
            return "ERROR: Falta el parametro -path";
        }

        // EJECUTAR rmdisk
        return rmdisk(path);
    }

    // FDISK: Crear/eliminar/redimensionar particion
    else if (comando.find("fdisk") == 0) {
        int size = 0;
        string unit = "K";
        string path = "";
        string type = "P";
        string fit = "WF";
        string name = "";
        string delete_type = "";
        int add_size = 0;
        
        // EXTRAER PARAMETRO: -size
        size_t pos = comando.find("-size=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            size = stoi(valor.substr(0, valor.find(' ')));
        }
        
        // EXTRAER PARAMETRO: -unit
        pos = comando.find("-unit=");
        if (pos != string::npos) {
            unit = comando[pos + 6];
        }
        
        // EXTRAER PARAMETRO: -path
        pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        // EXTRAER PARAMETRO: -type
        pos = comando.find("-type=");
        if (pos != string::npos) {
            type = comando[pos + 6];
        }
        
        // EXTRAER PARAMETRO: -fit
        pos = comando.find("-fit=");
        if (pos != string::npos) {
            fit = comando.substr(pos + 5, 2);
        }
        
        // EXTRAER PARAMETRO: -name
        pos = comando.find("-name=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                name = valor.substr(1, cierre - 1);
            } else {
                name = valor.substr(0, valor.find(' '));
            }
        }
        
        // EXTRAER PARAMETRO: -delete (NUEVO)
        pos = comando.find("-delete=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 8);
            delete_type = valor.substr(0, valor.find(' '));
        }
        
        // EXTRAER PARAMETRO: -add
        pos = comando.find("-add=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 5);
            add_size = stoi(valor.substr(0, valor.find(' ')));
        }
        
        // Validar parametros
        if (path.empty()) return "ERROR: -path requerido";
        if (name.empty()) return "ERROR: -name requerido";
        
        // Si no es delete ni add, validar size
        if (delete_type.empty() && add_size == 0 && size <= 0) {
            return "ERROR: -size requerido para crear particion";
        }
        
        return fdisk(size, unit, path, type, fit, name, delete_type, add_size);
    }
    
    // MOUNTED: Muestra las particiones montadas en memoria
        else if (comando.find("mounted") == 0) {
        return mounted();
    }

    // MOUNT: Montar particion
    else if (comando.find("mount") == 0) {
        string path = "", name = "";
        
        // Extraer -path
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        // Extraer -name
        pos = comando.find("-name=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                name = valor.substr(1, cierre - 1);
            } else {
                name = valor.substr(0, valor.find(' '));
            }
        }
        
        if (path.empty() || name.empty()) {
            return "ERROR: Faltan parametros para MOUNT";
        }
        
        return mount(path, name);
    }

    // UNMOUNT: Desmontar particion
    else if (comando.find("unmount") == 0) {
        string id = "";
        
        size_t pos = comando.find("-id=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 4);
            id = valor.substr(0, valor.find(' '));
        }
        
        if (id.empty()) {
            return "ERROR: Falta parametro -id para UNMOUNT";
        }
        
        return unmount(id);
    }

    // MKFS: Formateo de la particion
    else if (comando.find("mkfs") == 0) {
        string id = "", type = "full", fs = "2fs";
        
        size_t pos = comando.find("-id=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 4);
            id = valor.substr(0, valor.find(' '));
        }
        
        pos = comando.find("-type=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            type = valor.substr(0, valor.find(' '));
        }
        
        pos = comando.find("-fs=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 4);
            fs = valor.substr(0, valor.find(' '));
        }
        
        if (id.empty()) {
            return "ERROR: Falta parametro -id para MKFS";
        }
        
        return mkfs(id, type, fs);
    }

    // LOGIN
    else if (comando.find("login") == 0) {
        string user = "", pass = "", id = "";
    
        size_t pos = comando.find("-user=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            user = valor.substr(0, valor.find(' '));
        }
    
        pos = comando.find("-pass=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            pass = valor.substr(0, valor.find(' '));
        }
        
        pos = comando.find("-id=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 4);
            id = valor.substr(0, valor.find(' '));
        }
    
        if (user.empty() || pass.empty() || id.empty()) {
            return "ERROR: Faltan parmetros para LOGIN";
        }
        
        return login(user, pass, id);
    }

    // LOGOUT
    else if (comando.find("logout") == 0) {
        return logout();
    }

    // MKGRP: Crea un grupo para los usuarios de la particion
    else if (comando.find("mkgrp") == 0) {
        string name = "";
        
        size_t pos = comando.find("-name=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                name = valor.substr(1, cierre - 1);
            } else {
                name = valor.substr(0, valor.find(' '));
            }
        }
        
        if (name.empty()) {
            return "ERROR: Falta parametro -name para MKGRP";
        }
        
        return mkgrp(name);
    }

    // RMGRP: Elimina un grupo para los usuarios de la particion
    else if (comando.find("rmgrp") == 0) {
        string name = "";
        
        size_t pos = comando.find("-name=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                name = valor.substr(1, cierre - 1);
            } else {
                name = valor.substr(0, valor.find(' '));
            }
        }
        
        if (name.empty()) {
            return "ERROR: Falta parametro -name para RMGRP";
        }
        
        return rmgrp(name);
    }

    // MKUSR: Crear usuario
    else if (comando.find("mkusr") == 0) {
        string user = "", pass = "", grp = "";
        
        size_t pos = comando.find("-user=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            user = valor.substr(0, valor.find(' '));
        }
        
        pos = comando.find("-pass=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            pass = valor.substr(0, valor.find(' '));
        }
        
        pos = comando.find("-grp=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 5);
            grp = valor.substr(0, valor.find(' '));
        }
        
        if (user.empty() || pass.empty() || grp.empty()) {
            return "ERROR: Faltan parametros para MKUSR";
        }
        
        return mkusr(user, pass, grp);
    }

    // RMUSR: Eliminar usuario
    else if (comando.find("rmusr") == 0) {
        string user = "";
        
        size_t pos = comando.find("-user=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            user = valor.substr(0, valor.find(' '));
        }
        
        if (user.empty()) {
            return "ERROR: Falta el parametro -user para RMUSR";
        }
    
        return rmusr(user);
    }

    // CHGRP: Cambia grupo de usuario
    else if (comando.find("chgrp") == 0) {
        string user = "", grp = "";
        
        size_t pos = comando.find("-user=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            user = valor.substr(0, valor.find(' '));
        }
        
        pos = comando.find("-grp=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 5);
            grp = valor.substr(0, valor.find(' '));
        }
        
        if (user.empty() || grp.empty()) {
            return "ERROR: Faltan parametros para CHGRP";
        }
        
        return chgrp(user, grp);
    }

    // MKFILE: Crear archivo
    else if (comando.find("mkfile") == 0) {
        string path = "";
        bool r = false;
        int size = 0;
        string cont = "";
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        if (comando.find("-r") != string::npos) {
            r = true;
        }
        
        pos = comando.find("-size=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            size = stoi(valor.substr(0, valor.find(' ')));
        }
        
        pos = comando.find("-cont=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                cont = valor.substr(1, cierre - 1);
            } else {
                cont = valor.substr(0, valor.find(' '));
            }
        }
        
        if (path.empty()) {
            return "ERROR: Falta parametro -path para MKFILE";
        }
        
        return mkfile(path, r, size, cont);
    }

    // MKDIR: Crear carpeta
    else if (comando.find("mkdir") == 0) {
        string path = "";
        bool p = false;
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        if (comando.find("-p") != string::npos) {
            p = true;
        }
        
        if (path.empty()) {
            return "ERROR: Falta parametro -path para MKDIR";
        }
        
        return mkdir(path, p);
    }

    // CAT: Mostrar contenido de archivos
    else if (comando.find("cat") == 0) {
        vector<string> archivos;
        
        // Buscar todos los parametros -file1, -file2, etc.
        for (int i = 1; i <= 10; i++) {
            string param = "-file" + to_string(i) + "=";
            size_t pos = comando.find(param);
            if (pos != string::npos) {
                string valor = comando.substr(pos + param.length());
                if (valor[0] == '"') {
                    size_t cierre = valor.find('"', 1);
                    archivos.push_back(valor.substr(1, cierre - 1));
                } else {
                    archivos.push_back(valor.substr(0, valor.find(' ')));
                }
            } else {
                break;
            }
        }
        
        if (archivos.empty()) {
            return "ERROR: Faltan parametros para CAT";
        }
        
        return cat(archivos);
    }

    // REMOVE: Eliminar archivo o carpeta
    else if (comando.find("remove") == 0) {
        string path = "";
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        if (path.empty()) {
            return "ERROR: Falta parametro -path para REMOVE";
        }
        
        return removeItem(path);
    }

    // COPY: Copiar archivo o carpeta
    else if (comando.find("copy") == 0) {
        string path = "", destino = "";
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        pos = comando.find("-destino=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 9);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                destino = valor.substr(1, cierre - 1);
            } else {
                destino = valor.substr(0, valor.find(' '));
            }
        }
        
        if (path.empty() || destino.empty()) {
            return "ERROR: Faltan parametros para COPY";
        }
        
        return copyItem(path, destino);
    }

    // MOVE: Mover archivo o carpeta
    else if (comando.find("move") == 0) {
        string path = "", destino = "";
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        pos = comando.find("-destino=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 9);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                destino = valor.substr(1, cierre - 1);
            } else {
                destino = valor.substr(0, valor.find(' '));
            }
        }
        
        if (path.empty() || destino.empty()) {
            return "ERROR: Faltan parametros para MOVE";
        }
        
        return moveItem(path, destino);
    }

    // RENAME: Cambiar nombre de archivo o carpeta
    else if (comando.find("rename") == 0) {
        string path = "";
        string nuevo_nombre = "";
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        pos = comando.find("-name=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                nuevo_nombre = valor.substr(1, cierre - 1);
            } else {
                nuevo_nombre = valor.substr(0, valor.find(' '));
            }
        }
        
        if (path.empty()) {
            return "ERROR: Falta parametro -path para RENAME";
        }
        if (nuevo_nombre.empty()) {
            return "ERROR: Falta parametro -name para RENAME";
        }
        
        return renameItem(path, nuevo_nombre);
    }

    // FIND: buscar archivos por nombre
    else if (comando.find("find") == 0) {
        string path = "", name = "";
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        pos = comando.find("-name=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                name = valor.substr(1, cierre - 1);
            } else {
                name = valor.substr(0, valor.find(' '));
            }
        }
        
        if (path.empty() || name.empty()) {
            return "ERROR: Faltan parametros para FIND";
        }
        
        return find(path, name);
    }

    // CHOWN: Cambiar propietario
    else if (comando.find("chown") == 0) {
        string path = "", usuario = "";
        bool recursivo = false;
        
        size_t pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        pos = comando.find("-usuario=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 9);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                usuario = valor.substr(1, cierre - 1);
            } else {
                usuario = valor.substr(0, valor.find(' '));
            }
        }
        
        if (comando.find("-r") != string::npos) {
            recursivo = true;
        }
        
        if (path.empty() || usuario.empty()) {
            return "ERROR: Faltan parametros para CHOWN";
        }
        
        return chown(path, usuario, recursivo);
    }

    // REP: Genaracion de reportes
    else if (comando.find("rep") == 0) {
        string name = "", path = "", id = "", path_file_ls = "";
        
        // EXTRAER PARAMETRO: -name
        size_t pos = comando.find("-name=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            name = valor.substr(0, valor.find(' '));
        }
        
        // EXTRAER PARAMETRO: -path
        pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path = valor.substr(1, cierre - 1);
            } else {
                path = valor.substr(0, valor.find(' '));
            }
        }
        
        // EXTRAER PARAMETRO: -id
        pos = comando.find("-id=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 4);
            id = valor.substr(0, valor.find(' '));
        }
        
        // EXTRAER PARAMETRO: -path_file_ls (opcional)
        pos = comando.find("-path_file_ls=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 14);
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                path_file_ls = valor.substr(1, cierre - 1);
            } else {
                path_file_ls = valor.substr(0, valor.find(' '));
            }
        }
        
        // VALIDAR PARAMETROS OBLIGATORIOS
        if (name.empty() || path.empty() || id.empty()) {
            return "ERROR: Faltan parametros para REP";
        }
        
        // Llamar a la funcion
        return rep(name, path, id, path_file_ls);
    }

    // Comando no reconocido
    return "ERROR: Comando no reconocido: '" + comando + "'. Comandos disponibles: mkdisk, rmdisk";
}

// --- FUNCION: loginWeb (para login desde interfaz grafica) ---
string loginWeb(string user, string pass, string id) {
    // 1. Buscar particion montada por ID
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return "ERROR: No existe particion montada con ID: " + id;
    }
    
    Montada& m = particiones_montadas[idx];
    
    // 2. Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // 3. Leer users.txt
    string users = leerArchivo(m.path_disco, sb.s_inode_start, sb.s_block_start, sb.s_bm_block_start, sb.s_blocks_count, "/users.txt");
    
    // 4. Buscar usuario en el archivo
    stringstream ss(users);
    string linea;
    
    while (getline(ss, linea)) {
        if (linea.empty()) continue;
        
        vector<string> partes;
        stringstream ss2(linea);
        string parte;
        while (getline(ss2, parte, ',')) {
            partes.push_back(parte);
        }
        
        // Formato: UID,U,GID,USER,PASS
        if (partes.size() >= 5 && partes[1] == "U" && partes[3] == user && partes[4] == pass) {
            sesion_actual.activa = true;
            sesion_actual.id_particion = id;
            sesion_actual.usuario = user;
            try {
                sesion_actual.uid = stoi(partes[0]);
                sesion_actual.gid = stoi(partes[2]);
            } catch (...) {
                sesion_actual.uid = -1;
                sesion_actual.gid = -1;
            }
            return "LOGIN: Sesion iniciada como " + user;
        }
    }
    
    return "ERROR: Usuario o contrasena incorrectos";
}

// --- FUNCION: obtenerDiscos ---
string obtenerDiscos() {
    stringstream res;
    res << "[";
    
    // Buscar archivos .mia en la carpeta discos
    string comando = "ls ../discos/*.mia 2>/dev/null";
    char buffer[128];
    string resultado = "";
    FILE* pipe = popen(comando.c_str(), "r");
    if (!pipe) {
        return "[]";
    }
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        resultado += buffer;
    }
    pclose(pipe);
    
    // Parsear resultado
    stringstream ss(resultado);
    string linea;
    vector<string> discos;
    while (getline(ss, linea)) {
        if (!linea.empty()) {
            // Eliminar el salto de línea correctamente
            if (linea.back() == '\n') linea.pop_back();
            if (linea.back() == '\r') linea.pop_back();
            discos.push_back(linea);
        }
    }
    
    for (int i = 0; i < discos.size(); i++) {
        res << "\"" << discos[i] << "\"";
        if (i < discos.size() - 1) res << ",";
    }
    
    res << "]";
    return res.str();
}

// --- FUNCION: listarDirectorio ---
string listarDirectorio(string id, string ruta) {
    // 1. Buscar particion montada por ID
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return "ERROR: No existe particion montada con ID: " + id;
    }
    
    Montada& m = particiones_montadas[idx];
    
    // 2. Obtener superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // 3. Obtener el inodo de la ruta
    int inodo_dir = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta);
    if (inodo_dir == -1) {
        return "ERROR: No existe la ruta: " + ruta;
    }
    
    Inodo dir_inodo;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_dir, dir_inodo)) {
        return "ERROR: No se pudo leer el inodo del directorio";
    }
    
    if (dir_inodo.i_type != 0) {
        return "ERROR: La ruta no es un directorio";
    }
    
    // 4. Generar el listado
    stringstream resultado;
    resultado << "Permisos | Owner | Grupo | Tamano | Tipo | Nombre\n";
    resultado << "------------------------------------------------\n";
    
    // Recorrer bloques del directorio
    for (int i = 0; i < 12 && dir_inodo.i_block[i] != -1; i++) {
        BloqueCarpeta bloque;
        if (!leerBloqueCarpeta(m.path_disco, sb.s_block_start, dir_inodo.i_block[i], bloque)) {
            continue;
        }
        
        for (int j = 0; j < 4; j++) {
            if (bloque.b_content[j].b_inodo != -1) {
                string nombre(bloque.b_content[j].b_name);
                if (nombre == "." || nombre == "..") continue;
                
                int inodo_hijo = bloque.b_content[j].b_inodo;
                Inodo hijo;
                if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_hijo, hijo)) {
                    continue;
                }
                
                // Permisos
                resultado << hijo.i_perm[0] << hijo.i_perm[1] << hijo.i_perm[2] << " | ";
                // Owner
                resultado << hijo.i_uid << " | ";
                // Grupo
                resultado << hijo.i_gid << " | ";
                // Tamano
                resultado << hijo.i_size << " | ";
                // Tipo
                resultado << (hijo.i_type == 0 ? "DIR" : "FILE") << " | ";
                // Nombre
                resultado << nombre << "\n";
            }
        }
    }
    
    return resultado.str();
}

// --- FUNCION TEMPORAL: debugCarpeta ---
string debugCarpeta(string id, string ruta) {
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    int inodo_dir = buscarInodoPorRuta(m.path_disco, sb.s_inode_start, sb.s_block_start, ruta);
    if (inodo_dir == -1) return "ERROR: No existe la ruta: " + ruta;
    
    Inodo dir_inodo;
    if (!leerInodo(m.path_disco, sb.s_inode_start, inodo_dir, dir_inodo)) {
        return "ERROR: No se pudo leer inodo";
    }
    
    stringstream resultado;
    resultado << "=== CONTENIDO DE " << ruta << " ===\n";
    
    for (int i = 0; i < 12 && dir_inodo.i_block[i] != -1; i++) {
        BloqueCarpeta bloque;
        if (leerBloqueCarpeta(m.path_disco, sb.s_block_start, dir_inodo.i_block[i], bloque)) {
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo != -1) {
                    resultado << "  [" << j << "] " << bloque.b_content[j].b_name 
                              << " -> inodo " << bloque.b_content[j].b_inodo << "\n";
                }
            }
        }
    }
    
    return resultado.str();
}

// --- FUNCION: obtenerJournalEntradas ---
string obtenerJournalEntradas(string id) {
    // Buscar particion montada
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return "ERROR: Particion no encontrada";
    }
    
    Montada& m = particiones_montadas[idx];
    
    // Leer superbloque
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    // Verificar que sea EXT3
    if (sb.s_filesystem_type != 3) {
        return "ERROR: La particion no es EXT3, no tiene journaling";
    }
    
    // Leer todas las entradas del journal
    stringstream resultado;
    resultado << "Operacion|Path|Contenido|Fecha\n";
    
    ifstream disco(m.path_disco, ios::binary);
    if (!disco.is_open()) {
        return "ERROR: No se pudo abrir el disco";
    }
    
    for (int i = 0; i < sb.s_inodes_count; i++) {
        Journal journal;
        disco.seekg(m.part_start + sizeof(Superblock) + (i * sizeof(Journal)));
        disco.read(reinterpret_cast<char*>(&journal), sizeof(Journal));
        
        if (journal.j_count == 1) {
            char fecha[30];
            time_t t = (time_t)journal.j_content.i_date;
            struct tm* tm_info = localtime(&t);
            strftime(fecha, 30, "%d/%m/%Y %H:%M:%S", tm_info);
            
            resultado << journal.j_content.i_operation << "|"
                     << journal.j_content.i_path << "|"
                     << journal.j_content.i_content << "|"
                     << fecha << "\n";
        }
    }
    
    disco.close();
    return resultado.str();
}

// --- FUNCION TEMPORAL: debugRaiz ---
string debugRaiz(string id) {
    int idx = -1;
    for (int i = 0; i < particiones_montadas.size(); i++) {
        if (particiones_montadas[i].id == id) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return "ERROR: Particion no encontrada";
    
    Montada& m = particiones_montadas[idx];
    
    Superblock sb;
    if (!obtenerSuperblock(m.path_disco, m.part_start, sb)) {
        return "ERROR: No se pudo leer superbloque";
    }
    
    stringstream resultado;
    resultado << "=== BLOQUES DE CARPETA RAIZ ===\n";
    
    Inodo raiz;
    if (!leerInodo(m.path_disco, sb.s_inode_start, 0, raiz)) {
        return "ERROR: No se pudo leer inodo raiz";
    }
    
    for (int i = 0; i < 12 && raiz.i_block[i] != -1; i++) {
        resultado << "Bloque " << i << " (posicion " << raiz.i_block[i] << "):\n";
        BloqueCarpeta bloque;
        if (leerBloqueCarpeta(m.path_disco, sb.s_block_start, raiz.i_block[i], bloque)) {
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo != -1) {
                    resultado << "  [" << j << "] " << bloque.b_content[j].b_name 
                              << " -> inodo " << bloque.b_content[j].b_inodo << "\n";
                }
            }
        }
    }
    
    return resultado.str();
}

// MAIN CON CORS 
int main() {
    crow::SimpleApp app;
    
    // Ruta OPTIONS para /ejecutar (preflight CORS)
    CROW_ROUTE(app, "/ejecutar").methods("OPTIONS"_method)([]() {
        crow::response res;
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });
    
    // Ruta POST para ejecutar comandos
    CROW_ROUTE(app, "/ejecutar").methods("POST"_method)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "JSON invalido");
        }
        
        string comando = body["comando"].s();
        string resultado = procesar_comando(comando);
        
        crow::json::wvalue respuesta;
        respuesta["resultado"] = resultado;
        respuesta["error"] = "";
        
        crow::response res(respuesta);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // Ruta POST para login desde interfaz grafica
    CROW_ROUTE(app, "/loginWeb")
        .methods("POST"_method)([](const crow::request& req){
            auto body = crow::json::load(req.body);
            if (!body) {
                return crow::response(400, "JSON invalido");
            }
            
            string user = body["user"].s();
            string pass = body["pass"].s();
            string id = body["id"].s();
            
            string resultado = loginWeb(user, pass, id);
            
            crow::json::wvalue respuesta;
            respuesta["resultado"] = resultado;
            
            return crow::response(respuesta);
        });

    // Ruta POST para listar directorio
    CROW_ROUTE(app, "/listarDirectorio")
        .methods("POST"_method)([](const crow::request& req){
            auto body = crow::json::load(req.body);
            if (!body) {
                return crow::response(400, "JSON invalido");
            }
            
            string id = body["id"].s();
            string ruta = body["ruta"].s();
            
            string resultado = listarDirectorio(id, ruta);
            
            crow::json::wvalue respuesta;
            respuesta["contenido"] = resultado;
            
            return crow::response(respuesta);
        });
        
    CROW_ROUTE(app, "/debugCarpeta").methods("POST"_method)([](const crow::request& req){
        auto body = crow::json::load(req.body);
        string id = body["id"].s();
        string ruta = body["ruta"].s();
        string resultado = debugCarpeta(id, ruta);
        crow::json::wvalue respuesta;
        respuesta["contenido"] = resultado;
        return crow::response(respuesta);
    });

    // Ruta GET para obtener lista de discos
    CROW_ROUTE(app, "/obtenerDiscos")
        .methods("GET"_method)([](){
            string discos = obtenerDiscos();
            crow::json::wvalue respuesta;
            respuesta["discos"] = crow::json::load(discos);
            return respuesta;
        });
    
    // Ruta GET /ping
    CROW_ROUTE(app, "/ping").methods("GET"_method)([]() {
        crow::response res("pong");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });
    
    // Ruta GET /
    CROW_ROUTE(app, "/").methods("GET"_method)([]() {
        crow::response res("Backend funcionando");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // Ruta para obtener entradas del journal
    CROW_ROUTE(app, "/obtenerJournal").methods("POST"_method)([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "JSON invalido");
        }
        
        string id = body["id"].s();
        string resultado = obtenerJournalEntradas(id);
        
        crow::json::wvalue respuesta;
        respuesta["contenido"] = resultado;
        return crow::response(respuesta);
    });

    // Ruta para debug
    CROW_ROUTE(app, "/debugRaiz").methods("POST"_method)([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "JSON invalido");
        }
        string id = body["id"].s();
        string resultado = debugRaiz(id);
        crow::json::wvalue respuesta;
        respuesta["contenido"] = resultado;
        return crow::response(respuesta);
    });
    
    // Mostar Info
    cout << "=========================================" << endl;
    cout << "      BACKEND CON CORS FUNCIONANDO" << endl;
    cout << "   Servidor en http://localhost:8080" << endl;
    cout << "=========================================" << endl;
    
    // Iniciar servidor en puerto 8080
    app.port(8080).multithreaded().run();

    return 0;
}