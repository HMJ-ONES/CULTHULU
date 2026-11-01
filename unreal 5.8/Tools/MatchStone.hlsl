// Original texture-free masonry. World coordinates prevent stretched block UVs.
struct StoneField {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float noise(float2 p) {
  float2 i=floor(p),f=frac(p); f=f*f*(3-2*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
 }
};
StoneField field;
float3 N=normalize(SurfaceN);
float3 a=abs(N);
float2 plane=a.z>.65?WorldP.xy:(a.x>a.y?WorldP.yz:WorldP.xz);
float2 scale=a.z>.65?float2(330,280):float2(420,190);
float2 uv=plane/scale;
uv.x+=floor(uv.y)*.5;
float2 cell=floor(uv),local=frac(uv);
float2 edge=min(local,1-local)*scale;
float distance=min(edge.x,edge.y);
float aa=max(fwidth(distance),.5);
float joint=1-smoothstep(2.5-aa,5+aa,distance);
float grain=field.noise(plane*.025);
float weather=field.noise(plane*.0007+float2(WorldP.z*.00012,7));
float block=field.hash(cell);
float stoneTone=.78+block*.30+weather*.16+(grain-.5)*.12;
float rockTone=.72+field.noise(plane*.0018)*.36+(grain-.5)*.10;
float tone=lerp(rockTone,lerp(stoneTone,.32,joint),saturate(Masonry));
float luma=dot(Tint.rgb,float3(.2126,.7152,.0722));
float3 stoneTint=lerp(luma.xxx,Tint.rgb,.55);
StoneRoughness=clamp(.89+(weather-.5)*.10+joint*.05,.78,.98);
// Surface-gradient bump changes lighting only, never the collision silhouette.
float height=(grain-.5)*.8-joint*1.6*saturate(Masonry);
float3 dx=ddx(WorldP),dy=ddy(WorldP);
float3 r1=cross(dy,N),r2=cross(N,dx);
float det=dot(dx,r1);
float3 grad=sign(det)*(ddx(height)*r1+ddy(height)*r2);
StoneNormal=normalize(max(abs(det),.0001)*N-grad);
return max(stoneTint*tone,float3(.008,.009,.01));
