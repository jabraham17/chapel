#ifndef LocaleTab_h
#define LocaleTab_h

#include <FL/Fl_Group.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>

class LocaleTab : public Fl_Group {
  
    Fl_Text_Display * display;
    Fl_Text_Buffer * buf;
 
    public:  
        LocaleTab(int x, int y, int w, int h, Fl_Text_Buffer *buf, const char *l = 0);
        ~LocaleTab();
        void appendText(char *c);
};

#endif
