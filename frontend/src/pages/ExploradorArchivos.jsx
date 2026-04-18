import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';

function ExploradorArchivos() {
  const [rutaActual, setRutaActual] = useState('/');
  const [contenido, setContenido] = useState([]);
  const [cargando, setCargando] = useState(true);
  const navigate = useNavigate();
  
  const particion = JSON.parse(localStorage.getItem('particionSeleccionada') || '{}');

  useEffect(() => {
    if (!particion.id) {
      navigate('/seleccionar-particion');
      return;
    }
    cargarContenido('/');
  }, []);

  const cargarContenido = async (ruta) => {
    setCargando(true);
    try {
      const response = await fetch('/api/ejecutar', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ comando: `rep -name=ls -path_file_ls="${ruta}" -id=${particion.id}` })
      });
      const data = await response.json();
      
      // Parsear el reporte LS
      setContenido([]);
    } catch (err) {
      console.error('Error al cargar contenido:', err);
    } finally {
      setCargando(false);
    }
  };

  const volverAParticiones = () => {
    navigate('/seleccionar-particion');
  };

  return (
    <div className="explorador-archivos-container">
      <div className="explorador-archivos-header">
        <h1>Explorador de Archivos</h1>
        <button onClick={volverAParticiones}>Volver a Particiones</button>
      </div>
      
      <div className="ruta-actual">
        <span>Ruta: {rutaActual}</span>
      </div>
      
      <div className="contenido">
        {cargando ? (
          <div className="loading">Cargando...</div>
        ) : (
          <div className="archivos-lista">
            <p>Explorador de archivos</p>
            <p>Particion: {particion.nombre} (ID: {particion.id})</p>
          </div>
        )}
      </div>
    </div>
  );
}

export default ExploradorArchivos;