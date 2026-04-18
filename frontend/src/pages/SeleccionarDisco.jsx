import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';

function SeleccionarDisco() {
  const [discos, setDiscos] = useState([]);
  const [cargando, setCargando] = useState(true);
  const [error, setError] = useState('');
  const navigate = useNavigate();

  useEffect(() => {
    cargarDiscos();
  }, []);

  const cargarDiscos = async () => {
    try {
      const response = await fetch('/api/obtenerDiscos');
      const data = await response.json();
      setDiscos(data.discos || []);
    } catch (err) {
      setError('Error al cargar discos');
    } finally {
      setCargando(false);
    }
  };

  const seleccionarDisco = (discoPath) => {
    localStorage.setItem('discoSeleccionado', discoPath);
    navigate('/seleccionar-particion');
  };

  const volverALogin = () => {
    localStorage.removeItem('sesion_activa');
    localStorage.removeItem('usuario');
    localStorage.removeItem('id_particion');
    navigate('/login');
  };

  if (cargando) {
    return <div className="loading">Cargando discos...</div>;
  }

  return (
    <div className="seleccionar-disco-container">
      <div className="seleccionar-disco-header">
        <h1>Seleccionar Disco</h1>
        <button onClick={volverALogin} className="logout-btn">Cerrar Sesion</button>
      </div>
      
      {error && <div className="error-message">{error}</div>}
      
      {discos.length === 0 ? (
        <div className="no-discos">
          <p>No hay discos disponibles</p>
          <p>Usa la terminal para crear discos con el comando mkdisk</p>
          <button onClick={() => navigate('/terminal')}>Ir a la Terminal</button>
        </div>
      ) : (
        <div className="discos-lista">
          {discos.map((disco, index) => (
            <div key={index} className="disco-tarjeta" onClick={() => seleccionarDisco(disco)}>
              <h3>Disco {index + 1}</h3>
              <p className="disco-ruta">{disco}</p>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

export default SeleccionarDisco;