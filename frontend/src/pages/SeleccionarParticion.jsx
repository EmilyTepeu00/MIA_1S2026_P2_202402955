import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';

function SeleccionarParticion() {
  const [particiones, setParticiones] = useState([]);
  const [cargando, setCargando] = useState(true);
  const [error, setError] = useState('');
  const navigate = useNavigate();
  
  const discoSeleccionado = localStorage.getItem('discoSeleccionado');

  useEffect(() => {
    const sesion = localStorage.getItem('sesion_activa');
    console.log('SeleccionarParticion - Sesion activa:', sesion);
    if (sesion !== 'true') {
      navigate('/login');
      return;
    }
    if (!discoSeleccionado) {
      navigate('/seleccionar-disco');
      return;
    }
    cargarParticiones();
  }, []);

  const cargarParticiones = async () => {
    try {
      const response = await fetch('/api/ejecutar', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ comando: 'mounted' })
      });
      const data = await response.json();
      console.log('Respuesta de mounted:', data);
      
      // Parsear la salida de mounted
      const lineas = data.resultado.split('\n');
      console.log('Lineas:', lineas);
      
      const montadas = [];
      let particionActual = {};
      
      for (const linea of lineas) {
        if (linea.startsWith('ID: ')) {
          if (particionActual.id) {
            montadas.push(particionActual);
          }
          particionActual = { id: linea.substring(4).trim() };
        } else if (linea.includes('Disco:')) {
          const partes = linea.split(':');
          if (partes.length >= 2) {
            particionActual.ruta = partes[1].trim();
          }
        } else if (linea.includes('Particion:')) {
          const partes = linea.split(':');
          if (partes.length >= 2) {
            particionActual.nombre = partes[1].trim();
          }
        } else if (linea.includes('Tipo:')) {
          const partes = linea.split(':');
          if (partes.length >= 2) {
            particionActual.tipo = partes[1].trim();
          }
        }
      }
      if (particionActual.id) {
        montadas.push(particionActual);
      }
      
      console.log('Particiones montadas:', montadas);
      console.log('Disco seleccionado:', discoSeleccionado);
      
      // Filtrar particiones del disco seleccionado
      const filtradas = montadas.filter(part => part.ruta === discoSeleccionado);
      console.log('Particiones filtradas:', filtradas);
      
      setParticiones(filtradas);
    } catch (err) {
      console.error('Error:', err);
      setError('Error al cargar particiones');
    } finally {
      setCargando(false);
    }
  };

  const seleccionarParticion = (particion) => {
    localStorage.setItem('particionSeleccionada', JSON.stringify(particion));
    navigate('/explorador-archivos');
  };

  const volverADiscos = () => {
    navigate('/seleccionar-disco');
  };

  if (cargando) {
    return <div className="loading">Cargando particiones...</div>;
  }

  return (
    <div className="seleccionar-particion-container">
      <div className="seleccionar-particion-header">
        <h1>Seleccionar Particion</h1>
        <button onClick={volverADiscos}>Volver a Discos</button>
      </div>
      
      {error && <div className="error-message">{error}</div>}
      
      {particiones.length === 0 ? (
        <div className="no-particiones">
          <p>No hay particiones montadas en este disco</p>
          <p>Usa la terminal para montar particiones con el comando mount</p>
          <p>Comando: mount -path=../discos/Disco1.mia -name=Particion1</p>
          <button onClick={() => navigate('/terminal')}>Ir a la Terminal</button>
        </div>
      ) : (
        <div className="particiones-lista">
          {particiones.map((particion, index) => (
            <div key={index} className="particion-tarjeta" onClick={() => seleccionarParticion(particion)}>
              <h3>{particion.nombre}</h3>
              <p>ID: {particion.id}</p>
              <p>Tipo: {particion.tipo}</p>
              <p>Ruta: {particion.ruta}</p>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

export default SeleccionarParticion;