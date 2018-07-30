#include "Context.h"

#include "Wrapper.h"
#include "Engine.h"

void CContext::Expose(void)
{
  py::class_<CContext, boost::noncopyable>("JSContext", py::no_init)
    .def(py::init<py::object>(py::arg("global") = py::object(), 
                              "create a new context base on global object"))
                  
    .add_property("securityToken", &CContext::GetSecurityToken, &CContext::SetSecurityToken)

    .def_readonly("locals", &CContext::GetGlobal, "Local variables within context")
    
    .add_static_property("entered", &CContext::GetEntered, 
                         "Returns the last entered context.")
    .add_static_property("current", &CContext::GetCurrent, 
                         "Returns the context that is on the top of the stack.")
    .add_static_property("inContext", &CContext::InContext,
                         "Returns true if V8 has a current context.")

    .def("eval", &CContext::Evaluate)

    .def("enter", &CContext::Enter, "Enter this context. "
         "After entering a context, all code compiled and "
         "run is compiled and run in this context.")
    .def("leave", &CContext::Leave, "Exit this context. "
         "Exiting the current context restores the context "
         "that was in place when entering the current context.")

    .def("__nonzero__", &CContext::IsEntered)
    ;

  py::objects::class_value_wrapper<boost::shared_ptr<CContext>, 
    py::objects::make_ptr_instance<CContext, 
    py::objects::pointer_holder<boost::shared_ptr<CContext>,CContext> > >();
}

CContext::CContext(v8::Handle<v8::Context> context)
{
  v8::HandleScope handle_scope;

  m_context = v8::Persistent<v8::Context>::New(context);
}

CContext::CContext(py::object global)
{
  v8::HandleScope handle_scope;

  m_context = v8::Context::New();

  v8::Context::Scope context_scope(m_context);

  if (global.ptr() != Py_None)
  {    
    m_context->Global()->Set(v8::String::NewSymbol("__proto__"), CPythonObject::Wrap(global));  
  }
}

py::object CContext::GetGlobal(void) 
{ 
  v8::HandleScope handle_scope;

  return CJavascriptObject::Wrap(m_context->Global()); 
}

py::str CContext::GetSecurityToken(void)
{
  v8::HandleScope handle_scope;
 
  v8::Handle<v8::Value> token = m_context->GetSecurityToken();

  if (token.IsEmpty())
  {
    return py::str(py::handle<>(Py_None));
  }
  else
  {
    v8::String::AsciiValue str(token->ToString());

    return py::str(*str, str.length());
  }  
}

void CContext::SetSecurityToken(py::str token)
{
  v8::HandleScope handle_scope;

  if (token.ptr() == Py_None) 
  {
    m_context->UseDefaultSecurityToken();
  }
  else
  {    
    m_context->SetSecurityToken(v8::String::New(py::extract<const char *>(token)()));  
  }
}

CContextPtr CContext::GetEntered(void) 
{ 
  v8::HandleScope handle_scope;

  return CContextPtr(new CContext(v8::Context::GetEntered())); 
}
CContextPtr CContext::GetCurrent(void) 
{ 
  v8::HandleScope handle_scope;

  return CContextPtr(new CContext(v8::Context::GetCurrent())); 
}

py::object CContext::Evaluate(const std::string& src) 
{ 
  CEngine engine;

  CScriptPtr script = engine.Compile(src);

  return script->Run(); 
}
