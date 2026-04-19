import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';

function ExploradorArchivos() {
  const [rutaActual, setRutaActual] = useState('/');
  const [contenido, setContenido] = useState([]);
  const [cargando, setCargando] = useState(true);
  const [error, setError] = useState('');
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
    setError('');
    try {
      const response = await fetch('/api/listarDirectorio', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: particion.id, ruta: ruta })
      });
      const data = await response.json();
      
      // Parsear el contenido del reporte LS
      const lineas = data.contenido.split('\n');
      const elementos = [];
      
      for (const linea of lineas) {
        // Formato: "permisos | owner | grupo | tamaño | fecha | tipo | nombre"
        if (linea.includes('|')) {
          const partes = linea.split('|');
          if (partes.length >= 7) {
            elementos.push({
              permisos: partes[0].trim(),
              owner: partes[1].trim(),
              grupo: partes[2].trim(),
              tamaño: partes[3].trim(),
              fecha: partes[4].trim(),
              tipo: partes[5].trim(),
              nombre: partes[6].trim()
            });
          }
        }
      }
      
      setContenido(elementos);
      setRutaActual(ruta);
    } catch (err) {
      setError('Error al cargar el contenido');
    } finally {
      setCargando(false);
    }
  };

  const navegarACarpeta = (nombre) => {
    let nuevaRuta = rutaActual === '/' ? '/' + nombre : rutaActual + '/' + nombre;
    cargarContenido(nuevaRuta);
  };

  const subirNivel = () => {
    if (rutaActual === '/') return;
    const partes = rutaActual.split('/');
    partes.pop();
    const nuevaRuta = partes.length === 1 ? '/' : partes.join('/');
    cargarContenido(nuevaRuta);
  };

  const volverAParticiones = () => {
    navigate('/seleccionar-particion');
  };

  if (cargando) {
    return <div className="loading">Cargando...</div>;
  }

  return (
    <div className="explorador-archivos-container">
      <div className="explorador-archivos-header">
        <h1>Explorador de Archivos</h1>
        <button onClick={volverAParticiones}>Volver a Particiones</button>
      </div>
      
      <div className="ruta-actual">
        <span>Ruta: {rutaActual}</span>
        {rutaActual !== '/' && (
          <button onClick={subirNivel} className="subir-btn">Subir nivel</button>
        )}
      </div>
      
      {error && <div className="error-message">{error}</div>}
      
      <div className="contenido-lista">
        {contenido.length === 0 ? (
          <div className="vacio">No hay elementos en este directorio</div>
        ) : (
          <table className="tabla-archivos">
            <thead>
              <tr>
                <th>Permisos</th>
                <th>Owner</th>
                <th>Grupo</th>
                <th>Tamaño</th>
                <th>Tipo</th>
                <th>Nombre</th>
              </tr>
            </thead>
            <tbody>
              {contenido.map((item, index) => (
                <tr 
                  key={index} 
                  onClick={() => item.tipo === 'DIR' && navegarACarpeta(item.nombre)}
                  className={item.tipo === 'DIR' ? 'carpeta-fila' : 'archivo-fila'}
                >
                  <td>{item.permisos}</td>
                  <td>{item.owner}</td>
                  <td>{item.grupo}</td>
                  <td>{item.tamaño}</td>
                  <td>{item.tipo}</td>
                  <td>{item.nombre}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}

export default ExploradorArchivos;