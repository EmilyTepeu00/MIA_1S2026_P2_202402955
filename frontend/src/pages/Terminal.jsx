import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';

function Terminal() {
  // Estados para los textareas
  const [comandos, setComandos] = useState('');
  const [salida, setSalida] = useState('');
  const navigate = useNavigate();

  // Verificar sesion al cargar
  console.log('=== TERMINAL CARGADA ===');
  console.log('Sesion activa:', localStorage.getItem('sesion_activa'));
  console.log('Usuario:', localStorage.getItem('usuario'));
  console.log('ID Particion:', localStorage.getItem('id_particion'));

  // Funcion para ejecutar comandos
  const ejecutarComandos = async () => {
    try {
      setSalida('Enviando comandos al backend...');
      
      // Separar comandos por lineas
      const lineas = comandos.split('\n').filter(linea => linea.trim() !== '');
      let resultadoFinal = '';
      
      for (const linea of lineas) {
        // Ignorar comentarios
        if (linea.trim().startsWith('#')) {
          resultadoFinal += linea + '\n';
          continue;
        }
        
        // Enviar al backend
        const response = await fetch('/api/ejecutar', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ comando: linea })
        });
        
        const data = await response.json();
        resultadoFinal += '> ' + linea + '\n' + data.resultado + '\n\n';
      }
      
      setSalida(resultadoFinal);
      
    } catch (error) {
      setSalida('ERROR de conexion: El backend esta activo?\n' + error);
    }
  };

  // Funcion para cargar archivo .smia
  const cargarScript = (event) => {
    const file = event.target.files[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (e) => {
        setComandos(e.target.result);
      };
      reader.readAsText(file);
    }
  };

  // FUNCIONES DE NAVEGACION
  const irALogin = () => navigate('/login');
  const irAExplorador = () => {
    console.log('Click en Explorador - Navegando a /seleccionar-disco');
    navigate('/seleccionar-disco');
  };
  const irAJournal = () => navigate('/journal');
  
  const cerrarSesion = async () => {
    await fetch('/api/ejecutar', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ comando: 'logout' })
    });
    localStorage.removeItem('sesion_activa');
    localStorage.removeItem('usuario');
    localStorage.removeItem('id_particion');
    console.log('Sesion cerrada');
    window.location.reload();
  };

  // Verificar si hay sesion activa
  const isAuthenticated = localStorage.getItem('sesion_activa') === 'true';
  console.log('isAuthenticated:', isAuthenticated);

  return (
    <div className="app">
      <h1>Proyecto 2: [C++Disk 2.0]</h1>
      
      {/* Botones de navegacion */}
      <div className="nav-buttons">
        {!isAuthenticated ? (
          <button onClick={irALogin} className="login-btn">Iniciar Sesion</button>
        ) : (
          <>
            <button onClick={irAExplorador}>Explorador</button>
            <button onClick={irAJournal}>Journaling</button>
            <button onClick={cerrarSesion} className="logout-btn">Cerrar Sesion</button>
          </>
        )}
      </div>
      
      <div className="container">
        <div className="input-area">
          <h2>Area de Comandos</h2>
          <textarea 
            value={comandos}
            onChange={(e) => setComandos(e.target.value)}
            rows="10"
            placeholder="Escribe los comandos aqui..."
          />
          
          <div className="buttons">
            <button onClick={() => document.getElementById('fileInput').click()}>
              Cargar Script
            </button>
            <input 
              type="file" 
              id="fileInput" 
              accept=".smia" 
              onChange={cargarScript}
              style={{ display: 'none' }}
            />
            <button onClick={ejecutarComandos}>
              Ejecutar
            </button>
          </div>
        </div>
        
        <div className="output-area">
          <h2>Area de Salida</h2>
          <textarea 
            value={salida}
            readOnly
            rows="10"
            placeholder="Resultados de la ejecucion..."
          />
        </div>
      </div>
    </div>
  );
}

export default Terminal;