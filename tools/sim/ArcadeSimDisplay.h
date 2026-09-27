#pragma once
#include "Arduino.h"
#define PROGMEM
#include "/root/Arduino/libraries/LovyanGFX/src/lgfx/Fonts/glcdfont.h"
namespace lgfx { struct swap565_t { uint16_t v; }; }
enum textdatum_t { top_left, top_center, middle_center, top_right };
namespace fonts { struct F{}; static F Font0; }
static uint16_t FB[240][320];
static inline uint16_t swp(uint16_t c){ return (c>>8)|(c<<8); }
class LGFX {
public:
  void init(){} void setRotation(int){} void setBrightness(int){} void fillScreen(uint16_t){}
  void startWrite(){} void endWrite(){} void waitDMA(){}
  void pushImageDMA(int x,int y,int w,int h, lgfx::swap565_t* d){ for(int r=0;r<h;r++) memcpy(&FB[y+r][x], (uint16_t*)d + r*w, w*2); }
};
class LGFX_Sprite {
  uint16_t* buf=nullptr; int W=0,H=0; int ts=1; textdatum_t dt=top_left; uint16_t tc=0xFFFF;
public:
  LGFX_Sprite(LGFX*){}
  LGFX_Sprite(){}
  void setColorDepth(int){} void setPsram(bool){}
  void* createSprite(int w,int h){ W=w;H=h; buf=new uint16_t[w*h](); return buf; }
  void* getBuffer(){ return buf; }
  int width(){ return W; } int height(){ return H; }
  void setFont(fonts::F*){}
  void setTextSize(int s){ ts=s; } void setTextDatum(textdatum_t d){ dt=d; } void setTextColor(uint16_t c){ tc=c; }
  void px(int x,int y,uint16_t c){ if(x>=0&&x<W&&y>=0&&y<H) buf[y*W+x]=swp(c); }
  void drawString(const char* s,int x,int y){
    int len=strlen(s), w=len*6*ts;
    if(dt==top_center) x-=w/2; if(dt==top_right) x-=w; if(dt==middle_center){ x-=w/2; y-=4*ts; }
    for(int i=0;i<len;i++){ const unsigned char* g=font+((unsigned char)s[i])*5;
      for(int cx=0;cx<5;cx++) for(int cy=0;cy<8;cy++) if(g[cx]&(1<<cy))
        for(int a=0;a<ts;a++) for(int b=0;b<ts;b++) px(x+(i*6+cx)*ts+a, y+cy*ts+b, tc); }
  }
  void drawCircle(int cx,int cy,int r,uint16_t c){ int x=r,y=0,e=0; while(x>=y){ px(cx+x,cy+y,c);px(cx+y,cy+x,c);px(cx-y,cy+x,c);px(cx-x,cy+y,c);px(cx-x,cy-y,c);px(cx-y,cy-x,c);px(cx+y,cy-x,c);px(cx+x,cy-y,c); y++; e+=1+2*y; if(2*(e-x)+1>0){x--; e+=1-2*x;} } }
  void fillCircle(int cx,int cy,int r,uint16_t c){ for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++) if(x*x+y*y<=r*r+r) px(cx+x,cy+y,c); }
  void drawLine(int x0,int y0,int x1,int y1,uint16_t c){ int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy; for(;;){ px(x0,y0,c); if(x0==x1&&y0==y1)break; int e2=2*e; if(e2>=dy){e+=dy;x0+=sx;} if(e2<=dx){e+=dx;y0+=sy;} } }
};
