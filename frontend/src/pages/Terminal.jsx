import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';

function Terminal() {
  // Estados para los textareas
  const [comandos, setComandos] = useState('');
  const [salida, setSalida] = useState('');
  const navigate = useNavigate();

  // EJECUTAR COMANDOS
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

  // PARA CARGAR ARCHIVO .smia
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

  // NAVEGACION
  const irAExplorador = () => navigate('/explorer');
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
    navigate('/login');
  };

  return (
    <div className="app">
      <h1>Proyecto 2: [C++Disk 2.0]</h1>
      
      <div className="nav-buttons">
        <button onClick={irAExplorador}>Explorador</button>
        <button onClick={irAJournal}>Journaling</button>
        <button onClick={cerrarSesion} className="logout-btn">Cerrar Sesion</button>
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