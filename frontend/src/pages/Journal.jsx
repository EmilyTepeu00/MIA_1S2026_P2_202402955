import React from 'react';
import { useNavigate } from 'react-router-dom';

function Journal() {
  const navigate = useNavigate();
  const usuario = localStorage.getItem('usuario');

  const handleLogout = () => {
    localStorage.removeItem('sesion_activa');
    localStorage.removeItem('usuario');
    localStorage.removeItem('id_particion');
    navigate('/login');
  };

  return (
    <div className="journal-container">
      <div className="journal-header">
        <h1>Journaling - Bitacora de Operaciones</h1>
        <div className="user-info">
          <span>Usuario: {usuario}</span>
          <button onClick={handleLogout}>Cerrar Sesión</button>
        </div>
      </div>
      <div className="journal-content">
        <p className="placeholder-message">
          registro de transicionessssssssssssss
        </p>
      </div>
    </div>
  );
}

export default Journal;