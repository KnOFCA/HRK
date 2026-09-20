#include "platform/harmony/backend.h"
#include <algorithm>
#include <cstring>
#include <time.h>
namespace hrk::harmony {
void OHAudioBackend::release(){if(renderer_){OH_AudioRenderer_Stop(renderer_);OH_AudioRenderer_Release(renderer_);renderer_=nullptr;}running_=false;}
Error OHAudioBackend::create(){
  OH_AudioStreamBuilder* builder=nullptr;
  if(OH_AudioStreamBuilder_Create(&builder,AUDIOSTREAM_TYPE_RENDERER)!=AUDIOSTREAM_SUCCESS)return Error::BackendFailure;
  OH_AudioRenderer_Callbacks callbacks{};callbacks.OH_AudioRenderer_OnWriteData=write;callbacks.OH_AudioRenderer_OnInterruptEvent=interrupt;callbacks.OH_AudioRenderer_OnError=error;
  bool ok=OH_AudioStreamBuilder_SetSamplingRate(builder,static_cast<int32_t>(pcm_.sampleRate))==AUDIOSTREAM_SUCCESS;
  ok=ok && OH_AudioStreamBuilder_SetChannelCount(builder,static_cast<int32_t>(pcm_.channels))==AUDIOSTREAM_SUCCESS;
  ok=ok && OH_AudioStreamBuilder_SetSampleFormat(builder,AUDIOSTREAM_SAMPLE_S16LE)==AUDIOSTREAM_SUCCESS;
  ok=ok && OH_AudioStreamBuilder_SetEncodingType(builder,AUDIOSTREAM_ENCODING_TYPE_RAW)==AUDIOSTREAM_SUCCESS;
  ok=ok && OH_AudioStreamBuilder_SetLatencyMode(builder,AUDIOSTREAM_LATENCY_MODE_FAST)==AUDIOSTREAM_SUCCESS;
  ok=ok && OH_AudioStreamBuilder_SetRendererInfo(builder,AUDIOSTREAM_USAGE_GAME)==AUDIOSTREAM_SUCCESS;
  ok=ok && OH_AudioStreamBuilder_SetRendererCallback(builder,callbacks,this)==AUDIOSTREAM_SUCCESS;
  ok=ok && OH_AudioStreamBuilder_GenerateRenderer(builder,&renderer_)==AUDIOSTREAM_SUCCESS;
  OH_AudioStreamBuilder_Destroy(builder);return ok?Error::Ok:Error::BackendFailure;
}
Error OHAudioBackend::load(const Pcm& p){release();if(!p.frames() || p.sampleRate!=48000 || (p.channels!=1 && p.channels!=2))return Error::AudioInvalid;pcm_=p;cursor_=0;offset_=lastPosition_=0;failed_=false;interrupted_=false;return create();}
Error OHAudioBackend::start(){if(!renderer_ || OH_AudioRenderer_Start(renderer_)!=AUDIOSTREAM_SUCCESS)return Error::BackendFailure;running_=true;return Error::Ok;}
Error OHAudioBackend::pause(){if(!renderer_)return Error::BackendFailure;lastPosition_=position();if(OH_AudioRenderer_Pause(renderer_)!=AUDIOSTREAM_SUCCESS)return Error::BackendFailure;running_=false;return Error::Ok;}
Error OHAudioBackend::resume(){return start();}
Error OHAudioBackend::seek(TimeNs t){
  if(!validTime(t)||t>duration())return Error::InvalidArgument;
  bool wasRunning=running_;release();offset_=t;lastPosition_=t;cursor_=static_cast<uint64_t>(t)*pcm_.sampleRate/second;
  auto e=create();if(e!=Error::Ok)return e;return wasRunning?start():Error::Ok;
}
void OHAudioBackend::stop(){release();cursor_=0;offset_=lastPosition_=0;}
bool OHAudioBackend::clockSample(TimeNs& host,TimeNs& song) const{
  if(!running_ || !renderer_)return false;
  int64_t frames=0,timestamp=0;
  if(OH_AudioRenderer_GetTimestamp(renderer_,CLOCK_MONOTONIC,&frames,&timestamp)!=AUDIOSTREAM_SUCCESS || frames<0 || timestamp<0)return false;
  host=timestamp;song=offset_+frames*second/pcm_.sampleRate;return true;
}
TimeNs OHAudioBackend::position() const{
  if(!running_ || !renderer_)return lastPosition_;
  TimeNs host=0,song=0;
  if(clockSample(host,song)){
    const TimeNs elapsed=std::clamp(monotonicNow()-host,TimeNs{0},maxTime);
    const TimeNs submitted=static_cast<TimeNs>(cursor_.load(std::memory_order_acquire))*second/pcm_.sampleRate;
    lastPosition_=std::max(lastPosition_,std::min({duration(),submitted,song+elapsed}));
  }
  return lastPosition_;
}
int32_t OHAudioBackend::write(OH_AudioRenderer*,void* context,void* buffer,int32_t length){
  auto& self=*static_cast<OHAudioBackend*>(context);if(!buffer || length<=0)return 0;
  std::memset(buffer,0,static_cast<size_t>(length));
  const uint64_t frame=self.cursor_.load(std::memory_order_relaxed);
  const uint64_t count=std::min<uint64_t>(static_cast<uint64_t>(length)/(self.pcm_.channels*2),self.pcm_.frames()-std::min(frame,self.pcm_.frames()));
  if(count)std::memcpy(buffer,self.pcm_.samples.data()+frame*self.pcm_.channels,static_cast<size_t>(count)*self.pcm_.channels*2);
  self.cursor_.store(frame+count,std::memory_order_release);return 0;
}
int32_t OHAudioBackend::interrupt(OH_AudioRenderer*,void* c,OH_AudioInterrupt_ForceType,OH_AudioInterrupt_Hint){static_cast<OHAudioBackend*>(c)->interrupted_=true;return 0;}
int32_t OHAudioBackend::error(OH_AudioRenderer*,void* c,OH_AudioStream_Result){static_cast<OHAudioBackend*>(c)->failed_=true;return 0;}
void HarmonyInputBackend::touch(OH_NativeXComponent* component,void* window){
  OH_NativeXComponent_TouchEvent event{};uint64_t w=0,h=0;
  if(OH_NativeXComponent_GetTouchEvent(component,window,&event)!=OH_NATIVEXCOMPONENT_RESULT_SUCCESS || OH_NativeXComponent_GetXComponentSize(component,window,&w,&h)!=OH_NATIVEXCOMPONENT_RESULT_SUCCESS || !w || !h)return;
  auto submit=[&](int32_t id,OH_NativeXComponent_TouchEventType type,float x,float y,int64_t timestamp){
    InputPhase phase;
    switch(type){case OH_NATIVEXCOMPONENT_DOWN:phase=InputPhase::Down;break;case OH_NATIVEXCOMPONENT_MOVE:phase=InputPhase::Move;break;case OH_NATIVEXCOMPONENT_UP:phase=InputPhase::Up;break;case OH_NATIVEXCOMPONENT_CANCEL:phase=InputPhase::Cancel;break;default:return;}
    RawInputEvent raw{static_cast<uint32_t>(id),phase,{std::clamp(x/static_cast<float>(w),0.f,1.f),std::clamp(y/static_cast<float>(h),0.f,1.f)},timestamp};
    if(!queue_.push(raw))++dropped_;
  };
  // DOWN/UP designate only the changed pointer. MOVE includes each pointer's original timestamp.
  if(event.type==OH_NATIVEXCOMPONENT_MOVE || event.type==OH_NATIVEXCOMPONENT_CANCEL){
    for(uint32_t i=0;i<std::min<uint32_t>(event.numPoints,OH_NATIVE_XCOMPONENT_MAX_TOUCH_POINTS_NUMBER);++i){const auto& p=event.touchPoints[i];submit(p.id,event.type,p.x,p.y,p.timeStamp);}
  }else submit(event.id,event.type,event.x,event.y,event.timeStamp);
}
namespace {
GLuint shader(GLenum kind,const char* source){GLuint s=glCreateShader(kind);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){glDeleteShader(s);return 0;}return s;}
}
bool GlesRenderBackend::create(void* window,uint32_t w,uint32_t h){
  destroy();display_=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(display_==EGL_NO_DISPLAY || !eglInitialize(display_,nullptr,nullptr)){destroy();return false;}
  const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
  EGLConfig config=nullptr;EGLint count=0;const EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
  if(!eglChooseConfig(display_,attributes,&config,1,&count)||!count){destroy();return false;}
  context_=eglCreateContext(display_,config,EGL_NO_CONTEXT,contextAttributes);surface_=eglCreateWindowSurface(display_,config,reinterpret_cast<EGLNativeWindowType>(window),nullptr);
  if(context_==EGL_NO_CONTEXT||surface_==EGL_NO_SURFACE||!eglMakeCurrent(display_,surface_,surface_,context_)){destroy();return false;}
  const char* vs="#version 300 es\nlayout(location=0) in vec2 p;layout(location=1) in vec2 uv;out vec2 v;void main(){gl_Position=vec4(p,0.,1.);v=uv;}";
  const char* fs="#version 300 es\nprecision mediump float;in vec2 v;uniform vec4 color;uniform sampler2D image;out vec4 c;void main(){c=texture(image,v)*color;}";
  GLuint vertex=shader(GL_VERTEX_SHADER,vs),fragment=shader(GL_FRAGMENT_SHADER,fs);if(!vertex||!fragment){if(vertex)glDeleteShader(vertex);if(fragment)glDeleteShader(fragment);destroy();return false;}
  program_=glCreateProgram();glAttachShader(program_,vertex);glAttachShader(program_,fragment);glLinkProgram(program_);glDeleteShader(vertex);glDeleteShader(fragment);
  GLint linked=0;glGetProgramiv(program_,GL_LINK_STATUS,&linked);if(!linked){destroy();return false;}
  color_=glGetUniformLocation(program_,"color");glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);glBufferData(GL_ARRAY_BUFFER,24*sizeof(float),nullptr,GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));
  glGenTextures(1,&texture_);glBindTexture(GL_TEXTURE_2D,texture_);const uint8_t white[]={255,255,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);width_=w;height_=h;glViewport(0,0,static_cast<GLsizei>(w),static_cast<GLsizei>(h));const bool ok=glGetError()==GL_NO_ERROR;eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(!ok)destroy();return ok;
}
void GlesRenderBackend::destroy(){
  if(display_!=EGL_NO_DISPLAY){
    if(context_!=EGL_NO_CONTEXT && surface_!=EGL_NO_SURFACE){eglMakeCurrent(display_,surface_,surface_,context_);if(texture_)glDeleteTextures(1,&texture_);if(buffer_)glDeleteBuffers(1,&buffer_);if(vao_)glDeleteVertexArrays(1,&vao_);if(program_)glDeleteProgram(program_);}
    eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(surface_!=EGL_NO_SURFACE)eglDestroySurface(display_,surface_);if(context_!=EGL_NO_CONTEXT)eglDestroyContext(display_,context_);eglTerminate(display_);
  }
  display_=EGL_NO_DISPLAY;context_=EGL_NO_CONTEXT;surface_=EGL_NO_SURFACE;program_=buffer_=vao_=texture_=0;width_=height_=0;
}
void GlesRenderBackend::resize(uint32_t w,uint32_t h){width_=w;height_=h;if(begin()){glViewport(0,0,static_cast<GLsizei>(w),static_cast<GLsizei>(h));eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);}}
bool GlesRenderBackend::begin(){return display_!=EGL_NO_DISPLAY && surface_!=EGL_NO_SURFACE && eglMakeCurrent(display_,surface_,surface_,context_);}
void GlesRenderBackend::end(){eglSwapBuffers(display_,surface_);eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);}
void GlesRenderBackend::clear(Color c){glClearColor(c.r,c.g,c.b,c.a);glClear(GL_COLOR_BUFFER_BIT);}
void GlesRenderBackend::sprite(const SpriteDrawCommand& c){
  float left=2*c.position.x-1-c.size.x,right=left+2*c.size.x,top=1-2*c.position.y+c.size.y,bottom=top-2*c.size.y;
  const float vertices[]={left,top,0,0,left,bottom,0,1,right,bottom,1,1,left,top,0,0,right,bottom,1,1,right,top,1,0};
  glUseProgram(program_);glUniform4f(color_,c.color.r,c.color.g,c.color.b,c.color.a);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,c.texture?c.texture:texture_);glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(vertices),vertices);glDrawArrays(GL_TRIANGLES,0,6);
}
}
