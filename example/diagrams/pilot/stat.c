/* stationarity pilot: is a human-authored layout a local minimum of a standard aesthetic energy? */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
static int N,M,*EA,*EB; static double *W,*H,*X;
static double Wc=1.0,Wl=1.0,Wo=1.0,L,CAP=1e9; static double *X0;
static uint32_t st_;
static uint32_t r32(void){ st_^=st_<<13; st_^=st_>>17; st_^=st_<<5; return st_; }
static double uni(void){ return r32()/4294967296.0; }
static int seg(double ax,double ay,double bx,double by,double cx,double cy,double dx,double dy){
    double d1=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax), d2=(bx-ax)*(dy-ay)-(by-ay)*(dx-ax);
    double d3=(dx-cx)*(ay-cy)-(dy-cy)*(ax-cx), d4=(dx-cx)*(by-cy)-(dy-cy)*(bx-cx);
    return ((d1>0)!=(d2>0)) && ((d3>0)!=(d4>0));
}
static double TC,TL,TO;
static double energy(const double*x){
    double c=0,len=0,ov=0,anode=0; int i,j;
    for(i=0;i<M;i++){
        double ax=x[2*EA[i]],ay=x[2*EA[i]+1],bx=x[2*EB[i]],by=x[2*EB[i]+1];
        double d=sqrt((bx-ax)*(bx-ax)+(by-ay)*(by-ay));
        len+=(d-L)*(d-L)/(L*L);
        for(j=i+1;j<M;j++){
            if(EA[i]==EA[j]||EA[i]==EB[j]||EB[i]==EA[j]||EB[i]==EB[j]) continue;
            if(seg(ax,ay,bx,by,x[2*EA[j]],x[2*EA[j]+1],x[2*EB[j]],x[2*EB[j]+1])) c+=1;
        }
    }
    for(i=0;i<N;i++){ anode+=W[i]*H[i];
        for(j=i+1;j<N;j++){
            double ox=(W[i]+W[j])/2-fabs(x[2*i]-x[2*j]), oy=(H[i]+H[j])/2-fabs(x[2*i+1]-x[2*j+1]);
            if(ox>0&&oy>0) ov+=ox*oy;
        }}
    TC=c/(double)M; TL=len/(double)M; TO=ov/anode;
    return Wc*TC + Wl*TL + Wo*TO;
}
/* one-node hill climb, budget evals */
static double climb(double*x,long budget,long*used){
    double f=energy(x),jit=0.1; long ev=1,rej=0,pat=40+10*N; int k=0;
    double *y=malloc(2*N*sizeof*y);
    while(ev<budget){
        memcpy(y,x,2*N*sizeof*y);
        int i=k%N; k++;
        y[2*i]+=(uni()*2-1)*jit; y[2*i+1]+=(uni()*2-1)*jit;
        if(y[2*i]<0)y[2*i]=0; if(y[2*i]>1)y[2*i]=1;
        if(y[2*i+1]<0)y[2*i+1]=0; if(y[2*i+1]>1)y[2*i+1]=1;
        if(X0){ double dx=y[2*i]-X0[2*i], dy=y[2*i+1]-X0[2*i+1], dd=sqrt(dx*dx+dy*dy);
                if(dd>CAP){ y[2*i]=X0[2*i]+dx*CAP/dd; y[2*i+1]=X0[2*i+1]+dy*CAP/dd; } }
        double g=energy(y); ev++;
        if(g<f){ f=g; memcpy(x,y,2*N*sizeof*y); rej=0; }
        else { if(++rej>=pat){ jit*=0.5; rej=0; if(jit<1e-6) jit=0.1; } }
    }
    free(y); *used=ev; return f;
}
int main(int argc,char**argv){
    /* stdin: n m ; n lines "x y w h" ; m lines "a b" */
    long budget=atol(argv[1]); unsigned seed=(unsigned)atoi(argv[2]);
    if(scanf("%d %d",&N,&M)!=2) return 1;
    X=malloc(2*N*sizeof*X); W=malloc(N*sizeof*W); H=malloc(N*sizeof*H);
    EA=malloc(M*sizeof*EA); EB=malloc(M*sizeof*EB);
    for(int i=0;i<N;i++) if(scanf("%lf %lf %lf %lf",&X[2*i],&X[2*i+1],&W[i],&H[i])!=4) return 1;
    for(int i=0;i<M;i++) if(scanf("%d %d",&EA[i],&EB[i])!=2) return 1;
    { double *d=malloc(M*sizeof*d); int i;
      for(i=0;i<M;i++){double dx=X[2*EA[i]]-X[2*EB[i]],dy=X[2*EA[i]+1]-X[2*EB[i]+1]; d[i]=sqrt(dx*dx+dy*dy);}
      for(i=0;i<M;i++)for(int j=i+1;j<M;j++) if(d[j]<d[i]){double t=d[i];d[i]=d[j];d[j]=t;}
      L = (argc>3 && argv[3][0]=='h') ? (d[M/2]>0?d[M/2]:1.0/sqrt((double)N)) : 1.0/sqrt((double)N);
      free(d); }
    st_=seed?seed:1;
    double *h=malloc(2*N*sizeof*h), *r=malloc(2*N*sizeof*r), *b=malloc(2*N*sizeof*b);
    memcpy(h,X,2*N*sizeof*h);
    double e_h=energy(h); double hc=TC,hl=TL,ho=TO;
    if(argc>4){ CAP=atof(argv[4]); X0=malloc(2*N*sizeof*X0); memcpy(X0,X,2*N*sizeof*X0); }
    long used;
    double e_hd=climb(h,budget,&used);                        /* descent from human */
    X0=NULL; for(int i=0;i<2*N;i++) r[i]=uni();
    double e_r0=energy(r);
    double e_rd=climb(r,budget,&used);                        /* descent from random */
    /* matched-budget uniform sampling control */
    double e_ctl=1e300;
    for(long t=0;t<budget;t++){ for(int i=0;i<2*N;i++) b[i]=uni(); double f=energy(b); if(f<e_ctl) e_ctl=f; }
    double dc,dl,do_; energy(h); dc=TC; dl=TL; do_=TO;
    printf("%d %d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
           N,M,e_h,e_hd,e_r0,e_rd,e_ctl,hc,hl,ho,dc,dl,do_,L);
    return 0;
}
