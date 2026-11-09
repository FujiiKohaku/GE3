#pragma once
// Game-side adapter. All GPU drawing remains in the existing engine classes.
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/input/Input.h"
#include "Engine/audio/SoundManager.h"
#include <map>
#include <filesystem>
#include <memory>
#include <functional>
#include <cstring>
#include <algorithm>
namespace KamataEngine {
struct AlmaVector2 { float x=0,y=0; };
struct AlmaVector3 { float x=0,y=0,z=0; };
struct AlmaVector4 { float x=1,y=1,z=1,w=1; };
struct AlmaMatrix4x4 { float m[4][4]{}; };
inline ::Vector3 Native(AlmaVector3 v) { return {v.x,v.y,v.z}; }
inline ::Vector4 Native(AlmaVector4 v) { return {v.x,v.y,v.z,v.w}; }
inline ::Matrix4x4 Native(const AlmaMatrix4x4& m) { ::Matrix4x4 r; std::memcpy(&r,&m,sizeof(r)); return r; }
inline AlmaMatrix4x4 Legacy(const ::Matrix4x4& m) { AlmaMatrix4x4 r; std::memcpy(&r,&m,sizeof(r)); return r; }
inline bool backgroundDrawing=false;
inline bool enhanced=true;
inline std::vector<std::function<void()>> uiDraws;
inline std::string Resource(const std::string& s) { return "resources/AlmaRun/"+s; }
struct AlmaWorldTransform {
 AlmaVector3 scale_{1,1,1},rotation_{},translation_{};
 AlmaMatrix4x4 matWorld_{};
 const AlmaWorldTransform* parent_=nullptr;
 void Initialize() { matWorld_=Legacy(MatrixMath::MakeIdentity4x4()); }
 void TransferMatrix() {}
};
class AlmaCamera {
public:
 ::Camera native;
 AlmaVector3 translation_{0,0,-50},rotation_{};
 float fovAngleY=0.785398163f,nearZ=0.1f,farZ=1000;
 AlmaMatrix4x4 matView{},matProjection{};
 void Initialize() { native.Initialize(); UpdateMatrix(); }
 void UpdateMatrix() {
  native.SetTranslate(Native(translation_)); native.SetRotate(Native(rotation_));
  native.SetFovY(fovAngleY); native.SetFarClip(farZ); native.SetNearClip(nearZ); native.Update();
  matView=Legacy(native.GetViewMatrix()); matProjection=Legacy(native.GetProjectionMatrix());
  Object3dManager::GetInstance()->SetDefaultCamera(&native);
 }
 void TransferMatrix() { UpdateMatrix(); }
};
class AlmaDebugCamera {
 AlmaCamera camera;
public:
 AlmaDebugCamera(int,int) {camera.Initialize();}
 void Update() {camera.UpdateMatrix();}
 AlmaCamera& GetCamera() {return camera;}
};
class AlmaObjectColor {
public:
 AlmaVector4 color{};
 void Initialize() {}
 void SetColor(AlmaVector4 c) {color=c;}
};
class AlmaTextureManager {
public:
 inline static std::vector<std::string> paths{Resource("white1x1.png")};
 static uint32_t Load(const std::string& p) {
  auto s=Resource(p); auto it=std::find(paths.begin(),paths.end(),s);
  if(it!=paths.end()) return uint32_t(it-paths.begin());
  ::TextureManager::GetInstance()->LoadTexture(s); paths.push_back(s); return uint32_t(paths.size()-1);
 }
};
class AlmaModel {
 std::string path;
 ModelCommon common;
 struct Instance { std::unique_ptr<Object3d> object; };
 std::map<uint32_t,std::unique_ptr<::Model>> texturedModels;
 std::map<std::pair<size_t,uint32_t>,Instance> instances;
 std::map<uint32_t,size_t> cursors;
 inline static std::vector<AlmaModel*> live;

public:
 AlmaModel(){live.push_back(this);}
 ~AlmaModel(){std::erase(live,this);}
 static void BeginFrame(){for(auto* m:live)m->cursors.clear();}
 static AlmaModel* CreateFromOBJ(const std::string& name,bool=false) {
  auto m=std::make_unique<AlmaModel>();
  m->common.Initialize(::DirectXCommon::GetInstance());
  // ModelManager resolves keys relative to resources/Models.
  m->path="../AlmaRun/"+name+"/"+name+".obj";
  ModelManager::GetInstance()->Load(m->path);
  return m.release();
 }
 static void PreDraw(ID3D12GraphicsCommandList*) {Object3dManager::GetInstance()->PreDraw();}
 static void PostDraw() {}
 void Draw(const AlmaWorldTransform& wt,AlmaCamera& camera,AlmaObjectColor* color=nullptr) {Draw(wt,camera,UINT32_MAX,color);}
 void Draw(const AlmaWorldTransform& wt,AlmaCamera& camera,uint32_t tex,AlmaObjectColor* color=nullptr) {
  auto& i=instances[{cursors[tex]++,tex}];
  if(!i.object) {
   i.object=std::make_unique<Object3d>(); i.object->Initialize(Object3dManager::GetInstance());
   if(tex!=UINT32_MAX) {
    auto& model=texturedModels[tex];
    if(!model){
     auto data=ModelManager::GetInstance()->Load(path)->GetModelData();
     for(auto& mat:data.materials) mat.textureFilePath=AlmaTextureManager::paths.at(tex);
     model=std::make_unique<::Model>(); model->Initialize(&common,data);
    }
    i.object->SetModel(model.get());
   } else i.object->SetModel(path);
   i.object->SetFrustumCullingEnabled(false);
  }
  i.object->SetCamera(&camera.native); i.object->SetCustomWorldMatrix(Native(wt.matWorld_));
  auto tint=color?Native(color->color) : ::Vector4{1,1,1,1};
  i.object->SetColor(tint); i.object->SetTransparent(tint.w<0.999f);
  bool startPrompt=path.find("/rReady/")!=std::string::npos || path.find("/Go!/")!=std::string::npos;
  bool decoration=path.find("skydome")!=std::string::npos || path.find("sora/")!=std::string::npos;
  i.object->SetEnableLighting(enhanced&&!decoration&&!startPrompt);
  i.object->SetSurfaceProperties(0.72f,0.0f,0.25f); i.object->SetFogEnabled(enhanced&&!decoration&&!startPrompt);
  i.object->Update(); i.object->Draw();
 }
};
class AlmaSprite {
 std::unique_ptr<::Sprite> sprite;
 std::unique_ptr<Object3d> background;
 inline static size_t backgroundLayer=0;
 std::vector<std::unique_ptr<::Sprite>> repeats;
 std::string texturePath;
 size_t renderCursor=0;
 inline static std::vector<AlmaSprite*> live;
 AlmaVector2 position{},size{1,1},anchor{},left{},texSize{};
 AlmaVector4 color{}; float rotation=0;
 void RenderBackground(){
  auto* camera=Object3dManager::GetInstance()->GetDefaultCamera();
  if(!camera)return;
  if(!background){
   background=std::make_unique<Object3d>();
   background->Initialize(Object3dManager::GetInstance());
   background->SetModel(ModelManager::GetInstance()->CreatePlane(texturePath));
   background->SetEnableLighting(false);
   background->SetFogEnabled(false);
   background->SetTransparent(true);
   background->SetTransparentDepthWrite(false);
   background->SetCastShadow(false);
   background->SetReceiveShadow(false);
   background->SetRaytracingEnabled(false);
   background->SetFrustumCullingEnabled(false);
  }
  // Place the existing textured plane behind gameplay, preserving pixel layout.
  float depth=camera->GetFarClip()*0.9f-float(backgroundLayer++)*0.1f;
  float unit=2.0f*depth*std::tan(camera->GetFovY()*0.5f)/720.0f;
  Vector3 center{(position.x+size.x*(0.5f-anchor.x)-640.0f)*unit,
   -(position.y+size.y*(0.5f-anchor.y)-360.0f)*unit,depth};
  auto local=MatrixMath::MakeAffineMatrix(Vector3{size.x*unit,size.y*unit,1},Vector3{0,0,-rotation},center);
  background->SetCamera(camera);
  background->SetCustomWorldMatrix(MatrixMath::Multiply(local,camera->GetWorldMatrix()));
  background->SetColor(Native(color));
  background->Update();background->Draw();
 }
 void Render(AlmaVector2 p,AlmaVector2 s,AlmaVector4 c,AlmaVector2 a,AlmaVector2 l,AlmaVector2 ts,float r) {
  ::Sprite* target=sprite.get();
  if(renderCursor>0){
   size_t index=renderCursor-1;
   if(index==repeats.size()){auto next=std::make_unique<::Sprite>();next->Initialize(SpriteManager::GetInstance(),texturePath);repeats.push_back(std::move(next));}
   target=repeats[index].get();
  }
  ++renderCursor;
  SpriteManager::GetInstance()->PreDraw(); target->SetPosition({p.x,p.y});target->SetSize({s.x,s.y});
  target->SetColor(Native(c));target->SetAnchorPoint({a.x,a.y});target->SetRotation(r);
  target->SetTextureLeftTop({l.x,l.y});target->SetTextureSize({ts.x,ts.y});target->Update();target->Draw();
 }
public:
 AlmaSprite(){live.push_back(this);}
 ~AlmaSprite(){std::erase(live,this);}
 static void BeginFrame(){backgroundLayer=0;for(auto* s:live)s->renderCursor=0;}
 static AlmaSprite* Create(uint32_t tex,AlmaVector2 p,AlmaVector4 c={1,1,1,1},AlmaVector2 a={0,0}) {
  auto* s=new AlmaSprite; s->texturePath=AlmaTextureManager::paths.at(tex); s->sprite=std::make_unique<::Sprite>(); s->sprite->Initialize(SpriteManager::GetInstance(),AlmaTextureManager::paths.at(tex));
  auto z=s->sprite->GetSize(); s->size={z.x,z.y}; auto t=s->sprite->GetTextureSize();s->texSize={t.x,t.y};s->position=p;s->color=c;s->anchor=a;return s;
 }
 static void PreDraw(ID3D12GraphicsCommandList*) {}
 static void PostDraw() {}
 void SetPosition(AlmaVector2 v){position=v;} void SetSize(AlmaVector2 v){size=v;}
 void SetColor(AlmaVector4 v){color=v;} void SetAnchorPoint(AlmaVector2 v){anchor=v;}
 void SetRotation(float v){rotation=v;} void SetTextureRect(AlmaVector2 p,AlmaVector2 s){left=p;texSize=s;}
 AlmaVector2 GetSize() const{return size;}
 void Draw(){
  auto f=[this,p=position,s=size,c=color,a=anchor,l=left,t=texSize,r=rotation]{Render(p,s,c,a,l,t,r);};
  if(backgroundDrawing) RenderBackground(); else uiDraws.push_back(f);
 }
};
class AlmaInput {
public:
 static AlmaInput* GetInstance(){static AlmaInput i;return &i;}
 bool PushKey(BYTE k){return ::Input::GetInstance()->IsKeyPressed(k);}
 bool TriggerKey(BYTE k){return ::Input::GetInstance()->IsKeyTrigger(k);}
};
class AlmaAudio {
 struct Pending {uint32_t sound;float volume;};
 std::vector<SoundData> sounds; std::map<std::string,uint32_t> soundHandles; std::map<uint32_t,Pending> pending;uint32_t next=1;
public:
 static AlmaAudio* GetInstance(){static AlmaAudio a;return &a;}
 uint32_t LoadWave(const std::string& p){if(soundHandles.contains(p))return soundHandles[p];sounds.push_back(SoundManager::GetInstance()->SoundLoadFile(Resource(p)));auto h=uint32_t(sounds.size()-1);soundHandles[p]=h;return h;}
 uint32_t PlayWave(uint32_t sound,bool=false){auto h=next++;pending[h]={sound,1};return h;}
 void SetVolume(uint32_t h,float v){if(pending.contains(h))pending[h].volume=v;}
 void Flush(){for(auto& [h,p]:pending)SoundManager::GetInstance()->SoundPlayWave(sounds.at(p.sound),p.volume);pending.clear();}
};
class AlmaDirectXCommon {
public:
 static AlmaDirectXCommon* GetInstance(){static AlmaDirectXCommon d;return &d;}
 ID3D12GraphicsCommandList* GetCommandList(){return ::DirectXCommon::GetInstance()->GetCommandList();}
 void ClearDepthBuffer() {} // The engine owns scene depth and frame boundaries.
};
class AlmaDebugText {public:static AlmaDebugText* GetInstance(){static AlmaDebugText d;return &d;}void DrawAll(){} void Print(const char*,int,int,float){} };
// Warning beams use the existing sprite renderer, projected by the game camera.
class PrimitiveDrawer {
 AlmaCamera* camera=nullptr;
 std::vector<std::unique_ptr<AlmaSprite>> lines;
 size_t cursor=0;
public:
 static PrimitiveDrawer* GetInstance(){static PrimitiveDrawer p;return &p;}
 void Reset(){cursor=0;}
 void Finalize(){lines.clear();cursor=0;camera=nullptr;}
 void SetCamera(AlmaCamera* c){camera=c;}
 void DrawLine3d(AlmaVector3 a,AlmaVector3 b,AlmaVector4 color){
  if(!camera)return;
  auto p=camera->native.WorldToScreen(Native(a)),q=camera->native.WorldToScreen(Native(b));
  if(cursor==lines.size())lines.emplace_back(AlmaSprite::Create(0,{0,0}));
  auto& s=lines[cursor++];float dx=q.x-p.x,dy=q.y-p.y;
  s->SetPosition({p.x,p.y});s->SetAnchorPoint({0,0.5f});
  s->SetSize({std::sqrt(dx*dx+dy*dy),1});s->SetRotation(std::atan2(dy,dx));s->SetColor(color);s->Draw();
 }
};

}
