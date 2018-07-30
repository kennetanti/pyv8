#include "Wrapper.h"
#include "irri_fix.h"

#include <vector>

#include "Context.h"

std::ostream& operator <<(std::ostream& os, const CJavascriptObject& obj)
{ 
  obj.Dump(os);

  return os;
}

void CWrapper::Expose(void)
{
  py::class_<CJavascriptObject, boost::noncopyable>("JSObject", py::no_init)
    .def_readonly("__js__", &CJavascriptObject::Native)

    .def("__getattr__", &CJavascriptObject::GetAttr)
    .def("__setattr__", &CJavascriptObject::SetAttr)
    .def("__delattr__", &CJavascriptObject::DelAttr)    

    .def_readonly("__members__", &CJavascriptObject::GetAttrList)

    .def(int_(py::self))
    .def(float_(py::self))
    .def(str(py::self))

    .def("__nonzero__", &CJavascriptObject::operator bool)
    .def("__eq__", &CJavascriptObject::Equals)
    .def("__ne__", &CJavascriptObject::Unequals)
    ;

  py::class_<CJavascriptArray, py::bases<CJavascriptObject>, boost::noncopyable>("JSArray", py::no_init)
    .def(py::init<size_t>())
    .def(py::init<py::list>())    

    .def("__len__", &CJavascriptArray::Length)

    .def("__getitem__", &CJavascriptArray::GetItem)
    .def("__setitem__", &CJavascriptArray::SetItem)
    .def("__delitem__", &CJavascriptArray::DelItem)

    .def("__iter__", py::range(&CJavascriptArray::begin, &CJavascriptArray::end))

    .def("__contains__", &CJavascriptArray::Contains)
    ;

  py::class_<CJavascriptFunction, py::bases<CJavascriptObject>, boost::noncopyable>("JSFunction", py::no_init)
    .def("__call__", &CJavascriptFunction::Invoke, 
         (py::arg("args") = py::list(), 
          py::arg("kwds") = py::dict()))
    .def("apply", &CJavascriptFunction::Apply, 
         (py::arg("self"), 
          py::arg("args") = py::list(), 
          py::arg("kwds") = py::dict()))
    .add_property("func_name", &CJavascriptFunction::GetName)
    .add_property("func_owner", &CJavascriptFunction::GetOwner)
    ;

  py::objects::class_value_wrapper<boost::shared_ptr<CJavascriptObject>, 
    py::objects::make_ptr_instance<CJavascriptObject, 
    py::objects::pointer_holder<boost::shared_ptr<CJavascriptObject>,CJavascriptObject> > >();
}

void CPythonObject::ThrowIf(void)
{
  assert(::PyErr_Occurred());
  
  v8::HandleScope handle_scope;

  PyObject *exc, *val, *trb;  

  ::PyErr_Fetch(&exc, &val, &trb);

  py::object type(py::handle<>(py::allow_null(exc))),
             value(py::handle<>(py::allow_null(val))),
             traceback(py::handle<>(py::allow_null(trb)));
  
  std::string msg;

  if (::PyObject_HasAttrString(value.ptr(), "message"))
  {
    py::extract<const std::string> extractor(value.attr("message"));

    if (extractor.check()) msg = extractor();
  }

  v8::Handle<v8::Value> error;

  if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_IndexError))
  {
    error = v8::Exception::RangeError(v8::String::New(msg.c_str(), msg.size()));
  }
  else if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_AttributeError))
  {
    error = v8::Exception::ReferenceError(v8::String::New(msg.c_str(), msg.size()));
  }
  else if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_SyntaxError))
  {
    error = v8::Exception::SyntaxError(v8::String::New(msg.c_str(), msg.size()));
  }
  else if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_TypeError))
  {
    error = v8::Exception::TypeError(v8::String::New(msg.c_str(), msg.size()));
  }
  else
  {
    error = v8::Exception::Error(v8::String::New(msg.c_str(), msg.size()));
  }

  v8::ThrowException(error);
}

#define TRY_HANDLE_EXCEPTION() try {
#define END_HANDLE_EXCEPTION(result) } \
  catch (const std::exception& ex) { v8::ThrowException(v8::Exception::Error(v8::String::New(ex.what()))); } \
  catch (const py::error_already_set&) { ThrowIf(); } \
  catch (...) { v8::ThrowException(v8::Exception::Error(v8::String::New("unknown exception"))); } \
  return result;

v8::Handle<v8::Value> CPythonObject::NamedGetter(
  v8::Local<v8::String> prop, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()
  
  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());  

  v8::String::AsciiValue name(prop);

  py::str attr_name(*name, name.length());

  if (!::PyObject_HasAttr(obj.ptr(), attr_name.ptr()))
    return v8::Local<v8::Value>();

  v8::Handle<v8::Value> result = Wrap(obj.attr(*name));

  return handle_scope.Close(result);

  END_HANDLE_EXCEPTION(v8::Undefined())
}

v8::Handle<v8::Value> CPythonObject::NamedSetter(
  v8::Local<v8::String> prop, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());

  v8::String::AsciiValue name(prop);

  py::str attr_name(*name, name.length());

  obj.attr(*name) = CJavascriptObject::Wrap(value);

  return value;
 
  END_HANDLE_EXCEPTION(v8::Undefined());
}

v8::Handle<v8::Boolean> CPythonObject::NamedQuery(
  v8::Local<v8::String> prop, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());  

  v8::String::AsciiValue name(prop);

  py::str attr_name(*name, name.length());

  return v8::Boolean::New(::PyObject_HasAttr(obj.ptr(), attr_name.ptr()));

  END_HANDLE_EXCEPTION(v8::False())
}

v8::Handle<v8::Boolean> CPythonObject::NamedDeleter(
  v8::Local<v8::String> prop, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());  

  v8::String::AsciiValue name(prop);

  py::str attr_name(*name, name.length());
  
  return v8::Boolean::New(::PyObject_DelAttr(obj.ptr(), attr_name.ptr()));
  
  END_HANDLE_EXCEPTION(v8::False())
}

v8::Handle<v8::Value> CPythonObject::IndexedGetter(
  uint32_t index, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());  

  py::object ret(py::handle<>(::PySequence_GetItem(obj.ptr(), index)));

  return handle_scope.Close(Wrap(ret));  
  
  END_HANDLE_EXCEPTION(v8::Undefined())
}
v8::Handle<v8::Value> CPythonObject::IndexedSetter(
  uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());  

  int ret = ::PySequence_SetItem(obj.ptr(), index, CJavascriptObject::Wrap(value).ptr());

  if (ret < 0) 
    v8::ThrowException(v8::Exception::Error(v8::String::New("fail to set indexed value")));

  return value;
  
  END_HANDLE_EXCEPTION(v8::Undefined())
}
v8::Handle<v8::Boolean> CPythonObject::IndexedQuery(
  uint32_t index, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());  

  return v8::Boolean::New(index < ::PySequence_Size(obj.ptr()));
  
  END_HANDLE_EXCEPTION(v8::False())
}
v8::Handle<v8::Boolean> CPythonObject::IndexedDeleter(
  uint32_t index, const v8::AccessorInfo& info)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object obj = CJavascriptObject::Wrap(info.Holder());  

  v8::Handle<v8::Value> value = IndexedGetter(index, info);

  return v8::Boolean::New(0 <= ::PySequence_DelItem(obj.ptr(), index));
  
  END_HANDLE_EXCEPTION(v8::False())
}

v8::Handle<v8::Value> CPythonObject::Caller(const v8::Arguments& args)
{
  TRY_HANDLE_EXCEPTION()

  v8::HandleScope handle_scope;

  py::object self;
  
  if (args.Data().IsEmpty())
  {
    self = CJavascriptObject::Wrap(args.This());
  }
  else
  {
    v8::Handle<v8::External> field = v8::Handle<v8::External>::Cast(args.Data());

    self = *static_cast<py::object *>(field->Value());
  }

  py::object result;

  switch (args.Length())
  {
  case 0: result = self(); break;
  case 1: result = self(CJavascriptObject::Wrap(args[0])); break;
  case 2: result = self(CJavascriptObject::Wrap(args[0]), CJavascriptObject::Wrap(args[1])); break;
  case 3: result = self(CJavascriptObject::Wrap(args[0]), CJavascriptObject::Wrap(args[1]), 
                        CJavascriptObject::Wrap(args[2])); break;
  case 4: result = self(CJavascriptObject::Wrap(args[0]), CJavascriptObject::Wrap(args[1]), 
                        CJavascriptObject::Wrap(args[2]), CJavascriptObject::Wrap(args[3])); break;
  case 5: result = self(CJavascriptObject::Wrap(args[0]), CJavascriptObject::Wrap(args[1]), 
                        CJavascriptObject::Wrap(args[2]), CJavascriptObject::Wrap(args[3]),
                        CJavascriptObject::Wrap(args[4])); break;
  case 6: result = self(CJavascriptObject::Wrap(args[0]), CJavascriptObject::Wrap(args[1]), 
                        CJavascriptObject::Wrap(args[2]), CJavascriptObject::Wrap(args[3]),
                        CJavascriptObject::Wrap(args[4]), CJavascriptObject::Wrap(args[5])); break;
  default:
    return v8::ThrowException(v8::Exception::Error(v8::String::New("too many arguments")));
  }

  return handle_scope.Close(Wrap(result));
  
  END_HANDLE_EXCEPTION(v8::Undefined())
}

void CPythonObject::SetupObjectTemplate(v8::Handle<v8::ObjectTemplate> clazz)
{
  clazz->SetInternalFieldCount(1);
  clazz->SetNamedPropertyHandler(cazt(v8::NamedPropertyGetter, NamedGetter), NamedSetter, cazt(v8::NamedPropertyQuery, NamedQuery), NamedDeleter);
  clazz->SetIndexedPropertyHandler(cazt(v8::IndexedPropertyGetter, IndexedGetter), IndexedSetter, cazt(v8::IndexedPropertyQuery, IndexedQuery), IndexedDeleter);
  clazz->SetCallAsFunctionHandler(Caller);
}

v8::Persistent<v8::ObjectTemplate> CPythonObject::CreateObjectTemplate(void)
{
  v8::HandleScope handle_scope;

  v8::Handle<v8::ObjectTemplate> clazz = v8::ObjectTemplate::New();

  SetupObjectTemplate(clazz);

  return v8::Persistent<v8::ObjectTemplate>::New(clazz);
}

#define TRY_CONVERT(type, cls) { py::extract<type> extractor(obj); \
  if (extractor.check()) return handle_scope.Close(cls::New(extractor())); }

v8::Handle<v8::Value> CPythonObject::Wrap(py::object obj)
{
  assert(v8::Context::InContext());

  v8::HandleScope handle_scope;

  if (obj.ptr() == Py_None) return v8::Null();
  if (obj.ptr() == Py_True) return v8::True();
  if (obj.ptr() == Py_False) return v8::False();

  TRY_CONVERT(int, v8::Int32);
  TRY_CONVERT(const char *, v8::String);
  TRY_CONVERT(bool, v8::Boolean);
  TRY_CONVERT(double, v8::Number);  

  py::extract<CJavascriptObject&> extractor(obj);

  if (extractor.check())
  {
    return handle_scope.Close(extractor().Object());
  }

  v8::Handle<v8::Value> result;

  if (PyFunction_Check(obj.ptr()) || PyMethod_Check(obj.ptr()) || PyType_Check(obj.ptr()))
  {
    v8::Handle<v8::FunctionTemplate> func_tmpl = v8::FunctionTemplate::New();    

    func_tmpl->SetCallHandler(Caller, v8::External::New(new py::object(obj)));
    
    if (PyType_Check(obj.ptr()))
    {
      v8::Handle<v8::String> cls_name = v8::String::New(py::extract<const char *>(obj.attr("__name__"))());

      func_tmpl->SetClassName(cls_name);
    }

    result = func_tmpl->GetFunction();
  }
  else if (PyBool_Check(obj.ptr()))
  {
    result = v8::Boolean::New(py::extract<bool>(obj));
  }
  else if (PyString_Check(obj.ptr()))
  {
    result = v8::String::New(PyString_AS_STRING(obj.ptr()));
  }
  else if (PyUnicode_Check(obj.ptr()))
  {
    result = v8::String::New(reinterpret_cast<const uint16_t *>(PyUnicode_AS_UNICODE(obj.ptr())));
  }
  else if (PyNumber_Check(obj.ptr()))
  {   
    result = v8::Number::New(py::extract<double>(obj));
  }
  else
  {
    static v8::Persistent<v8::ObjectTemplate> s_template = CreateObjectTemplate();

    v8::Handle<v8::Object> instance = s_template->NewInstance();

    v8::Handle<v8::External> payload = v8::External::New(new py::object(obj));

    instance->SetInternalField(0, payload);

    result = instance;
  }

  return handle_scope.Close(result);
}

void CJavascriptObject::CheckAttr(v8::Handle<v8::String> name) const
{
  assert(v8::Context::InContext());

  if (!m_obj->Has(name))
  {
    std::ostringstream msg;
      
    msg << "'" << *v8::String::AsciiValue(m_obj->ObjectProtoToString()) 
        << "' object has no attribute '" << *v8::String::AsciiValue(name) << "'";

    throw CJavascriptException(msg.str(), ::PyExc_AttributeError);
  }
}

py::object CJavascriptObject::GetAttr(const std::string& name)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  v8::Handle<v8::String> attr_name = v8::String::New(name.c_str());

  CheckAttr(attr_name);

  v8::Handle<v8::Value> attr_value = m_obj->Get(attr_name);

  if (attr_value.IsEmpty()) 
    CJavascriptException::ThrowIf(try_catch);

  return CJavascriptObject::Wrap(attr_value, m_obj);
}

void CJavascriptObject::SetAttr(const std::string& name, py::object value)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  v8::Handle<v8::String> attr_name = v8::String::New(name.c_str());
  v8::Handle<v8::Value> attr_obj = CPythonObject::Wrap(value);

  if (!m_obj->Set(attr_name, attr_obj)) 
    CJavascriptException::ThrowIf(try_catch);
}
void CJavascriptObject::DelAttr(const std::string& name)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  v8::Handle<v8::String> attr_name = v8::String::New(name.c_str());

  CheckAttr(attr_name);
  
  if (!m_obj->Delete(attr_name)) 
    CJavascriptException::ThrowIf(try_catch);
}
py::list CJavascriptObject::GetAttrList(void)
{
  py::list attrs;

  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  v8::Handle<v8::Array> props = m_obj->GetPropertyNames();

  for (size_t i=0; i<props->Length(); i++)
  {    
    attrs.append(CJavascriptObject::Wrap(props->Get(v8::Integer::New(i))));
  }

  if (try_catch.HasCaught()) CJavascriptException::ThrowIf(try_catch);

  return attrs;
}

bool CJavascriptObject::Equals(CJavascriptObjectPtr other) const
{
  v8::HandleScope handle_scope;

  return m_obj->Equals(other->m_obj);
}

void CJavascriptObject::Dump(std::ostream& os) const
{
  v8::HandleScope handle_scope;

  if (m_obj.IsEmpty())
    os << "None";
  else if (m_obj->IsInt32())
    os << m_obj->Int32Value();
  else if (m_obj->IsNumber())
    os << m_obj->NumberValue();
  else if (m_obj->IsBoolean())
    os << m_obj->BooleanValue();
  else if (m_obj->IsNull())
    os << "None";
  else if (m_obj->IsUndefined())
    os << "N/A";
  else if (m_obj->IsString())  
    os << *v8::String::AsciiValue(v8::Handle<v8::String>::Cast(m_obj));  
  else 
    os << *v8::String::AsciiValue(m_obj->ToString());
}

CJavascriptObject::operator long() const 
{ 
  v8::HandleScope handle_scope;

  if (m_obj.IsEmpty())
    throw CJavascriptException("argument must be a string or a number, not 'NoneType'", ::PyExc_TypeError);

  return m_obj->Int32Value(); 
}
CJavascriptObject::operator double() const 
{ 
  v8::HandleScope handle_scope;

  if (m_obj.IsEmpty())
    throw CJavascriptException("argument must be a string or a number, not 'NoneType'", ::PyExc_TypeError);

  return m_obj->NumberValue(); 
}

CJavascriptObject::operator bool() const
{
  v8::HandleScope handle_scope;

  if (m_obj.IsEmpty()) return false;

  return m_obj->BooleanValue();
}

py::object CJavascriptObject::Wrap(v8::Handle<v8::Value> value, v8::Handle<v8::Object> self)
{
  assert(v8::Context::InContext());

  v8::HandleScope handle_scope;

  if (value.IsEmpty() || value->IsNull()) return py::object(py::handle<>(Py_None));
  if (value->IsTrue()) return py::object(py::handle<>(Py_True));
  if (value->IsFalse()) return py::object(py::handle<>(Py_False));

  if (value->IsInt32()) return py::object(value->Int32Value());  
  if (value->IsString())
  {
    v8::String::AsciiValue str(v8::Handle<v8::String>::Cast(value));

    return py::str(*str, str.length());
  }
  if (value->IsBoolean()) return py::object(py::handle<>(value->BooleanValue() ? Py_True : Py_False));
  if (value->IsNumber()) return py::object(py::handle<>(::PyFloat_FromDouble(value->NumberValue())));

  return Wrap(value->ToObject(), self);
}

py::object CJavascriptObject::Wrap(v8::Handle<v8::Object> obj, v8::Handle<v8::Object> self) 
{
  v8::HandleScope handle_scope;

  if (obj.IsEmpty())
  {
    return py::object(py::handle<>(Py_None));
  }
  else if (obj->IsArray())
  {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(obj);

    return Wrap(new CJavascriptArray(array));
  }
  else if (obj->IsFunction())
  {
    v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(obj);

    if (func->InternalFieldCount() == 1)
    {
      v8::Handle<v8::External> field = v8::Handle<v8::External>::Cast(func->GetInternalField(0));

      return *static_cast<py::object *>(field->Value());
    }

    return Wrap(new CJavascriptFunction(self, func));
  }
  else if (obj->IsObject() && obj->InternalFieldCount() == 1)
  {
    v8::Handle<v8::External> field = v8::Handle<v8::External>::Cast(obj->GetInternalField(0));

    return *static_cast<py::object *>(field->Value());   
  }

  return Wrap(new CJavascriptObject(obj));
}

py::object CJavascriptObject::Wrap(CJavascriptObject *obj)
{
  return py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CJavascriptObject>(CJavascriptObjectPtr(obj))));
}

CJavascriptArray::CJavascriptArray(size_t size)  
{
  v8::HandleScope handle_scope;

  v8::Handle<v8::Array> array = v8::Array::New(size);

  m_obj = v8::Persistent<v8::Object>::New(array);  
}
CJavascriptArray::CJavascriptArray(py::list items)
{
  v8::HandleScope handle_scope;

  size_t size = ::PyList_Size(items.ptr());

  v8::Handle<v8::Array> array = v8::Array::New(size);

  for (size_t i=0; i<size; i++)
  {
    array->Set(v8::Integer::New(i), CPythonObject::Wrap(items[i]));
  }

  m_obj = v8::Persistent<v8::Object>::New(array);  
}
size_t CJavascriptArray::Length(void) const
{
  v8::HandleScope handle_scope;

  return v8::Handle<v8::Array>::Cast(m_obj)->Length();
}
py::object CJavascriptArray::GetItem(size_t idx)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  if (!m_obj->Has(idx))
  {
    std::ostringstream msg;

    msg << "'" << *v8::String::AsciiValue(m_obj->ObjectProtoToString()) 
        << "' index out of range";

    throw CJavascriptException(msg.str(), ::PyExc_IndexError);
  }
  
  v8::Handle<v8::Value> value = m_obj->Get(v8::Integer::New(idx));

  if (value.IsEmpty()) CJavascriptException::ThrowIf(try_catch);

  return CJavascriptObject::Wrap(value, m_obj);
}
py::object CJavascriptArray::SetItem(size_t idx, py::object value)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  if (!m_obj->Set(v8::Integer::New(idx), CPythonObject::Wrap(value)))
    CJavascriptException::ThrowIf(try_catch);

  return value;
}
py::object CJavascriptArray::DelItem(size_t idx)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  py::object value;

  if (m_obj->Has(idx))
    value = CJavascriptObject::Wrap(m_obj->Get(v8::Integer::New(idx)), m_obj);
  
  if (!m_obj->Delete(idx))
    CJavascriptException::ThrowIf(try_catch);

  return value;
}

bool CJavascriptArray::Contains(py::object item)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  for (size_t i=0; i<Length(); i++)
  {
    if (m_obj->Has(i) && item == GetItem(i))
    {
      return true;
    }
  }

  if (try_catch.HasCaught()) CJavascriptException::ThrowIf(try_catch);

  return false;
}

py::object CJavascriptFunction::Call(v8::Handle<v8::Object> self, py::list args, py::dict kwds)
{
  v8::HandleScope handle_scope;

  v8::TryCatch try_catch;

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(m_obj);

  std::vector< v8::Handle<v8::Value> > params(::PyList_Size(args.ptr()));

  for (size_t i=0; i<params.size(); i++)
  {
    params[i] = CPythonObject::Wrap(args[i]);
  }

  v8::Handle<v8::Value> result = func->Call(
    self.IsEmpty() ? v8::Context::GetCurrent()->Global() : self,
    params.size(), params.empty() ? NULL : &params[0]);

  if (result.IsEmpty()) CJavascriptException::ThrowIf(try_catch);

  return CJavascriptObject::Wrap(result->ToObject());
}
py::object CJavascriptFunction::Apply(CJavascriptObjectPtr self, py::list args, py::dict kwds)
{
  v8::HandleScope handle_scope;

  return Call(self->Object(), args, kwds);
}

py::object CJavascriptFunction::Invoke(py::list args, py::dict kwds) 
{ 
  v8::HandleScope handle_scope;

  return Call(m_self, args, kwds); 
}

const std::string CJavascriptFunction::GetName(void) const
{
  v8::HandleScope handle_scope;

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(m_obj);  

  v8::String::AsciiValue name(v8::Handle<v8::String>::Cast(func->GetName()));

  return std::string(*name, name.length());
}
