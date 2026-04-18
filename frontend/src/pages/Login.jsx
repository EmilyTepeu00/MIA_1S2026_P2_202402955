import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';

function Login() {
  const [user, setUser] = useState('');
  const [pass, setPass] = useState('');
  const [id, setId] = useState('');
  const [error, setError] = useState('');
  const navigate = useNavigate();

  const handleLogin = async (e) => {
    e.preventDefault();
    setError('');
    
    try {
      const response = await fetch('/api/ejecutar', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ comando: `login -user=${user} -pass=${pass} -id=${id}` })
      });
      
      const data = await response.json();
      
      if (data.resultado.includes('LOGIN')) {
        // Guardar sesion en localStorage
        localStorage.setItem('sesion_activa', 'true');
        localStorage.setItem('usuario', user);
        localStorage.setItem('id_particion', id);
        navigate('/terminal');
      } else {
        setError(data.resultado);
      }
    } catch (err) {
      setError('Error de conexion con el backend');
    }
  };

  return (
    <div className="login-container">
      <div className="login-card">
        <h1>Iniciar Sesión</h1>
        <form onSubmit={handleLogin}>
          <div className="form-group">
            <label>Usuario</label>
            <input 
              type="text" 
              value={user} 
              onChange={(e) => setUser(e.target.value)} 
              required 
            />
          </div>
          <div className="form-group">
            <label>Contraseña</label>
            <input 
              type="password" 
              value={pass} 
              onChange={(e) => setPass(e.target.value)} 
              required 
            />
          </div>
          <div className="form-group">
            <label>ID de Partición</label>
            <input 
              type="text" 
              value={id} 
              onChange={(e) => setId(e.target.value)} 
              placeholder="Ej: 551A" 
              required 
            />
          </div>
          {error && <div className="error-message">{error}</div>}
          <button type="submit">Ingresar</button>
        </form>
      </div>
    </div>
  );
}

export default Login;