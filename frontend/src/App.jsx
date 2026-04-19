// FRONTEND - App.jsx
import React from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import './App.css';
import Login from './pages/Login';
import Terminal from './pages/Terminal';
import SeleccionarDisco from './pages/SeleccionarDisco';
import SeleccionarParticion from './pages/SeleccionarParticion';
import ExploradorArchivos from './pages/ExploradorArchivos';
import Journal from './pages/Journal';

// COMPONENTE PRINCIPAL CON RUTAS
function App() {
  // Verificar si hay sesion activa
  const isAuthenticated = localStorage.getItem('sesion_activa') === 'true';
  console.log('App - isAuthenticated:', isAuthenticated);

  return (
    <Routes>
      {/* Terminal es la pagina principal (accesible sin login) */}
      <Route path="/" element={<Terminal />} />
      <Route path="/terminal" element={<Terminal />} />
      
      {/* Login (accesible sin login) */}
      <Route path="/login" element={<Login />} />
      
      {/* Rutas protegidas (requieren sesion) */}
      <Route path="/seleccionar-disco" element={
        (() => {
          console.log('Renderizando ruta /seleccionar-disco, isAuthenticated:', isAuthenticated);
          return isAuthenticated ? <SeleccionarDisco /> : <Navigate to="/login" />;
        })()
      } />
      <Route path="/seleccionar-particion" element={
        isAuthenticated ? <SeleccionarParticion /> : <Navigate to="/login" />
      } />
      <Route path="/explorador-archivos" element={
        isAuthenticated ? <ExploradorArchivos /> : <Navigate to="/login" />
      } />
      <Route path="/journal" element={
        isAuthenticated ? <Journal /> : <Navigate to="/login" />
      } />
    </Routes>
  );
}

export default App;