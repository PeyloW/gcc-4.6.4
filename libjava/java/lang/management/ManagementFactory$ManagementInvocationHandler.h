
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __java_lang_management_ManagementFactory$ManagementInvocationHandler__
#define __java_lang_management_ManagementFactory$ManagementInvocationHandler__

#pragma interface

#include <java/lang/Object.h>
#include <gcj/array.h>

extern "Java"
{
  namespace javax
  {
    namespace management
    {
        class MBeanServerConnection;
        class ObjectName;
    }
  }
}

class java::lang::management::ManagementFactory$ManagementInvocationHandler : public ::java::lang::Object
{

public:
  ManagementFactory$ManagementInvocationHandler(::javax::management::MBeanServerConnection *, ::javax::management::ObjectName *);
  virtual ::java::lang::Object * invoke(::java::lang::Object *, ::java::lang::reflect::Method *, JArray< ::java::lang::Object * > *);
private:
  ::java::lang::Object * translate(::java::lang::Object *, ::java::lang::reflect::Method *);
  ::javax::management::MBeanServerConnection * __attribute__((aligned(__alignof__( ::java::lang::Object)))) conn;
  ::javax::management::ObjectName * bean;
public:
  static ::java::lang::Class class$;
};

#endif // __java_lang_management_ManagementFactory$ManagementInvocationHandler__
