import React from 'react';
import { useNavigate } from 'react-router-dom';

function Explorer() {
  const navigate = useNavigate();
  const usuario = localStorage.getItem('usuario');

  const handleLogout = () => {
    localStorage.removeItem('sesion_activa');
    localStorage.removeItem('usuario');
    localStorage.removeItem('id_particion');
    navigate('/login');
  };

  return (
    <div className="explorer-container">
      <div className="explorer-header">
        <h1>Explorador de Archivos</h1>
        <div className="user-info">
          <span>Usuario: {usuario}</span>
          <button onClick={handleLogout}>Cerrar Sesión</button>
        </div>
      </div>
      <div className="explorer-content">
        <p className="placeholder-message">
          falta el explorador de archivosssssssssssssss
        </p>
      </div>
    </div>
  );
}

export default Explorer;