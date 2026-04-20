import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';

function Journal() {
  const [operaciones, setOperaciones] = useState([]);
  const [cargando, setCargando] = useState(true);
  const [error, setError] = useState('');
  const navigate = useNavigate();
  
  const usuario = localStorage.getItem('usuario');
  const idParticion = localStorage.getItem('id_particion');

  useEffect(() => {
    const sesion = localStorage.getItem('sesion_activa');
    if (sesion !== 'true') {
      navigate('/login');
      return;
    }
    cargarJournal();
  }, []);

  const cargarJournal = async () => {
    setCargando(true);
    try {
      const response = await fetch('/api/obtenerJournal', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: idParticion })
      });
      const data = await response.json();
      
      console.log('Respuesta del backend:', data);
      
      const lineas = data.contenido.split('\n');
      const items = [];
      
      for (let i = 1; i < lineas.length; i++) {
        const linea = lineas[i];
        if (linea.trim() === '') continue;
        
        const partes = linea.split('|');
        if (partes.length >= 4) {
          items.push({
            operacion: partes[0],
            path: partes[1],
            contenido: partes[2] || '',
            fecha: partes[3]
          });
        }
      }
      
      setOperaciones(items);
    } catch (err) {
      console.error('Error:', err);
      setError('Error al cargar el journal');
    } finally {
      setCargando(false);
    }
  };

  const volverATerminal = () => {
    navigate('/');
  };

  const handleLogout = () => {
    localStorage.removeItem('sesion_activa');
    localStorage.removeItem('usuario');
    localStorage.removeItem('id_particion');
    navigate('/login');
  };

  if (cargando) {
    return <div className="loading">Cargando...</div>;
  }

  return (
    <div className="explorador-archivos-container">
      <div className="explorador-archivos-header">
        <h1>Journaling - Bitácora de Operaciones</h1>
        <div style={{ display: 'flex', gap: '1rem', alignItems: 'center' }}>
          <span>Usuario: {usuario} | ID: {idParticion}</span>
          <button onClick={() => navigate('/')} className="regresar-btn">Regresar</button>
        </div>
      </div>
      
      <div className="contenido-lista">
        {error && <div className="error-message">{error}</div>}
        
        {operaciones.length === 0 ? (
          <div className="vacio">No hay operaciones registradas en el journal</div>
        ) : (
          <table className="tabla-archivos">
            <thead>
              <tr>
                <th>Operación</th>
                <th>Ruta</th>
                <th>Contenido</th>
                <th>Fecha y Hora</th>
              </tr>
            </thead>
            <tbody>
              {operaciones.map((op, index) => (
                <tr key={index}>
                  <td>{op.operacion}</td>
                  <td>{op.path}</td>
                  <td style={{ fontFamily: 'monospace', fontSize: '0.85rem' }}>{op.contenido}</td>
                  <td>{op.fecha}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}

export default Journal;