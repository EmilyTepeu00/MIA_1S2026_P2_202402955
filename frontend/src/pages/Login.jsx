import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';

function Login() {
  const [user, setUser] = useState('');
  const [pass, setPass] = useState('');
  const [id, setId] = useState('');
  const [error, setError] = useState('');
  const [cargando, setCargando] = useState(false);
  const navigate = useNavigate();

  const handleLogin = async (e) => {
    e.preventDefault();
    setError('');
    setCargando(true);
    
    try {
      const response = await fetch('/api/loginWeb', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user, pass, id })
      });
      
      const data = await response.json();
      console.log('Respuesta del backend:', data);
      
      if (data.resultado && data.resultado.includes('LOGIN')) {
        // Guardar sesion en localStorage
        localStorage.setItem('sesion_activa', 'true');
        localStorage.setItem('usuario', user);
        localStorage.setItem('id_particion', id);
        console.log('Sesion guardada, redirigiendo a la terminal');
        navigate('/');
      } else {
        setError(data.resultado || 'Error desconocido');
      }
    } catch (err) {
      console.error('Error de conexion:', err);
      setError('Error de conexion con el backend');
    } finally {
      setCargando(false);
    }
  };

  // Funcion para ir a la terminal sin login (para crear discos primero)
  const irATerminal = () => {
    navigate('/');
  };

  return (
    <div className="login-container">
      <div className="login-card">
        <h1>Iniciar Sesion</h1>
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
            <label>Contrasena</label>
            <input 
              type="password" 
              value={pass} 
              onChange={(e) => setPass(e.target.value)} 
              required 
            />
          </div>
          <div className="form-group">
            <label>ID de Particion</label>
            <input 
              type="text" 
              value={id} 
              onChange={(e) => setId(e.target.value)} 
              placeholder="Ej: 551A" 
              required 
            />
          </div>
          {error && <div className="error-message">{error}</div>}
          <button type="submit" disabled={cargando}>
            {cargando ? 'Verificando...' : 'Ingresar'}
          </button>
        </form>
        
        <div className="login-footer">
          <button onClick={irATerminal} className="terminal-btn">
            Ir a la Terminal
          </button>
        </div>
      </div>
    </div>
  );
}

export default Login;