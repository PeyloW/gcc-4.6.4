
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __javax_print_event_PrintJobEvent__
#define __javax_print_event_PrintJobEvent__

#pragma interface

#include <javax/print/event/PrintEvent.h>
extern "Java"
{
  namespace javax
  {
    namespace print
    {
        class DocPrintJob;
      namespace event
      {
          class PrintJobEvent;
      }
    }
  }
}

class javax::print::event::PrintJobEvent : public ::javax::print::event::PrintEvent
{

public:
  PrintJobEvent(::javax::print::DocPrintJob *, jint);
  virtual jint getPrintEventType();
  virtual ::javax::print::DocPrintJob * getPrintJob();
private:
  static const jlong serialVersionUID = -1711656903622072997LL;
public:
  static const jint DATA_TRANSFER_COMPLETE = 106;
  static const jint JOB_CANCELED = 101;
  static const jint JOB_COMPLETE = 102;
  static const jint JOB_FAILED = 103;
  static const jint NO_MORE_EVENTS = 105;
  static const jint REQUIRES_ATTENTION = 104;
private:
  jint __attribute__((aligned(__alignof__( ::javax::print::event::PrintEvent)))) reason;
public:
  static ::java::lang::Class class$;
};

#endif // __javax_print_event_PrintJobEvent__
