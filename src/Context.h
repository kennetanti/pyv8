#pragma once

#include <cassert>

#include <boost/shared_ptr.hpp>

#include "Wrapper.h"

class CContext;

typedef boost::shared_ptr<CContext> CContextPtr;

class CContext 
{
  v8::Persistent<v8::Context> m_context;
public:
  CContext(v8::Handle<v8::Context> context);

  CContext(py::object global);

  ~CContext()
  {
    m_context.Dispose();
  }  

  v8::Handle<v8::Context> Handle(void) { return m_context; }

  py::object GetGlobal(void);

  py::str GetSecurityToken(void);
  void SetSecurityToken(py::str token);

  bool IsEntered(void) { return !m_context.IsEmpty(); }
  void Enter(void) { m_context->Enter(); }
  void Leave(void) { m_context->Exit(); }

  py::object Evaluate(const std::string& src);

  static CContextPtr GetEntered(void);
  static CContextPtr GetCurrent(void);
  static bool InContext(void) { return v8::Context::InContext(); }

  static void Expose(void);
};
