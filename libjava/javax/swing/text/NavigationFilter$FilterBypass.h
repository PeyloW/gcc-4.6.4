
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __javax_swing_text_NavigationFilter$FilterBypass__
#define __javax_swing_text_NavigationFilter$FilterBypass__

#pragma interface

#include <java/lang/Object.h>
extern "Java"
{
  namespace javax
  {
    namespace swing
    {
      namespace text
      {
          class Caret;
          class NavigationFilter$FilterBypass;
          class Position$Bias;
      }
    }
  }
}

class javax::swing::text::NavigationFilter$FilterBypass : public ::java::lang::Object
{

public:
  NavigationFilter$FilterBypass();
  virtual ::javax::swing::text::Caret * getCaret() = 0;
  virtual void moveDot(jint, ::javax::swing::text::Position$Bias *) = 0;
  virtual void setDot(jint, ::javax::swing::text::Position$Bias *) = 0;
  static ::java::lang::Class class$;
};

#endif // __javax_swing_text_NavigationFilter$FilterBypass__
