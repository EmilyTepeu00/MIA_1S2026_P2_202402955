#include <iostream>
#include <string>
#include <fstream>
#include <cstring>
#include <ctime>
#include <cstdlib>

using namespace std;

// --- ESTRUCTURAS DE DATOS ---

// ESTRUCTURA PARTITION
struct Partition {
    char part_status;   // "0" no montado y "1" montado
    char part_type;     // "P" primario, "E" extendico y "L" logica
    char part_fit;      // "B" best fit, "F" first fit y "W" = worst fit
    int part_start;     
    int part_size;
    char part_name[16];
    int part_correlative;   // -1 si es no montado
    int part_id[4];

    // Constructor
    Partition() {
        part_status = '0';
        part_type = 'P';
        part_fit = 'W';      // Worst fit por defecto
        part_start = -1;
        part_size = 0;
        memset(part_name, 0, sizeof(part_name));
        part_correlative = -1;
        memset(part_id, 0, sizeof(part_id));
    }
};

// ESTRUCTURA MBR
struct MBR {
    int mbr_size;
    time_t mbr_creation_date;
    int mbr_dsk_signature;      // Numero random del disco
    Partition mbr_partitions[4];

    // Constructor
    MBR() {
        mbr_size = 0;
        mbr_creation_date = time(nullptr); // Fecha actual
        mbr_dsk_signature = rand() % 10000;
    }
};
 
// --- FUNCION: mkdisk (para crear disco virtual) ---
string mkdisk(int size, string unit, string fit, string path){

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
    ofstream archivo(path.c_str(), ios::binary);

    // Verificar si se pudo crear ela archivo
    if (!archivo.is_open()) {
        return "ERROR: NO se pudo crear el archivo en la ruta: " + path;
    } 

    // LENAR EL ARCHIVO CON CEROS
    char buffer[1024] = {0};    // Buffer lleno de ceros
    int bytes_escritos = 0;

    while (bytes_escritos < total_bytes) {
        // Calcular cuantos bytes escribir
        int chunk = 1024;
        if (total_bytes - bytes_escritos < 1024) {
            chunk = total_bytes - bytes_escritos ;      // Ultimo fragmento
        }

        // Escribir el buffer en el archivo
        archivo.write(buffer, chunk);
        bytes_escritos += chunk;
    }

    // CREAR Y ESCRIBIR EL MBR
    MBR nuevo_mbr;
    nuevo_mbr.mbr_size =  total_bytes;

    // Determinar el caracter de fit
    char fit_char = 'F';
    if (fit == "BF") fit_char = 'B';
    else if (fit == "WF") fit_char = 'W';

    // Mover el puntero al inicio del arhcivo
    archivo.seekp(0, ios::beg);

    // Escribir el MBR en el archivo
    archivo.write(reinterpret_cast<const char*>(&nuevo_mbr), sizeof(MBR));
    archivo.close();

    // MENSAJE DE EXITO
    return string("MKDISK: Disco creado exitosamente\n") +
        "Ruta: " + path + "\n" +
        "Tamaño: " + to_string(total_bytes) + " bytes (" + 
        to_string(size) + " " + unit + ")\n" +
        "Fit: " + fit + "\n" +
        "Signature: " + to_string(nuevo_mbr.mbr_dsk_signature);
}

// --- FUNCION: procesar_comando ---
string procesar_comando(string comando) {

    // VERIFICAR SI ES EL COMANDO MKDISK
    if (comando.find("mkdisk") == 0) {

        int size = 0;
        string unit = "M";
        string fit = "FF";
        string path = "";

        // EXTRAER PARAMETRO: -size
        size_t pos = comando.find("-size=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            // Extraer solo el numero hasta el espacio
            size_t espacio = valor.find(' ');
            if (espacio != string::npos) {
                valor = valor.substr(0, espacio);
            }
            size = stoi(valor);
        }

        // EXTRAER PARAMETRO: -unit
        pos = comando.find("-unit=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            // Extraer solo la letra
            if (valor[0] == 'K' || valor[0] == 'M') {
                unit = valor[0];
            }
        }

        // EXTRAR PARAMETRO: -fit
        pos = comando.find("-fit=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 5);
            // Extraer los dos caracteres (BF, FF, WF)
            fit = valor.substr(0, 2);
        }

        // EXTRAER PARAMETRO: -path
        pos = comando.find("-path=");
        if (pos != string::npos) {
            string valor = comando.substr(pos + 6);
            
            // Verificar si hay comillas
            if (valor[0] == '"') {
                size_t cierre = valor.find('"', 1);
                if (cierre != string::npos) {
                    path = valor.substr(1, cierre - 1);
                }
            } else {
                // Si no hay comillas se toma hasta el espacio
                size_t espacio = valor.find(' ');
                if (espacio != string::npos) {
                    path = valor.substr(0, espacio);
                } else {
                    path = valor;  // Resto de la ruta
                }
            }
        }

        // VALIDAR LOS PARAMETROS OBLIGATORIOS
        if (size <= 0) {
            return "EROR: Faltan parametros";
        }
        if (path.empty()) {
            return "ERROR: Falta el parametro -path";
        }

        // EJECUTAR MKDISK
        return mkdisk(size, unit, fit, path);
    }

    // COMANDO NO RECONOCIO
    return "ERROR: Comando no reconocido: " + comando;
}

// FUNCION PRINCIPAL
int main() {
    
    // Mostrar banner de inicio
    cout << "   BACKEND - PROYECTO 1" << endl;
    
    string comando;
    
    while (true) {
        cout << endl << "> ";
        getline(cin, comando);
        
        // Salir
        if (comando == "exit" || comando == "salir") {
            cout << "Saliendo del programa..." << endl;
            break;
        }
        
        // Ignorar lineas vacias
        if (comando.empty()) {
            continue;
        }
        
        // Procesar el comando
        string resultado = procesar_comando(comando);
        
        // Mostrar resultado
        cout << resultado << endl;
    }
    
    return 0;
}
