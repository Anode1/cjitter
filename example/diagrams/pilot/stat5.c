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
    double *h=malloc(2*N*sizeof*h), *b=malloc(2*N*sizeof*b), *s=malloc(2*N*sizeof*s);
    memcpy(h,X,2*N*sizeof*h);
    double e_h=energy(h);
    double cap = (argc>4) ? atof(argv[4]) : 1e9;
    long used;

    /* A: the measurement. Capped descent from the human layout. */
    CAP=cap; X0=malloc(2*N*sizeof*X0); memcpy(X0,X,2*N*sizeof*X0);
    memcpy(h,X,2*N*sizeof*h);
    double e_hd=climb(h,budget,&used);

    /* Build a genuine local optimum: long UNCAPPED descent from the human, restarted until
     * a further pass cannot improve it. This is the reference point the null needs. */
    CAP=1e9; X0=NULL;
    memcpy(s,X,2*N*sizeof*s);
    double e_s=climb(s,budget*8,&used);
    for(int pass=0;pass<6;pass++){
        double before=e_s;
        e_s=climb(s,budget*4,&used);
        if(before-e_s < 1e-12*fabs(before)) break;
    }

    /* B: the null. The SAME capped descent, same budget, from that local optimum. Whatever
     * it removes is what the procedure strips from a layout that is already stationary. */
    CAP=cap; X0=malloc(2*N*sizeof*X0); memcpy(X0,s,2*N*sizeof*X0);
    memcpy(b,s,2*N*sizeof*b);
    double e_sd=climb(b,budget,&used);

    /* C: second null. Perturb the optimum by the cap, then the same capped descent. */
    double *p=malloc(2*N*sizeof*p);
    memcpy(p,s,2*N*sizeof*p);
    for(int i=0;i<N;i++){
        double a=uni()*6.283185307, rr=uni()*cap;
        p[2*i]+=rr*cos(a); p[2*i+1]+=rr*sin(a);
        if(p[2*i]<0)p[2*i]=0; if(p[2*i]>1)p[2*i]=1;
        if(p[2*i+1]<0)p[2*i+1]=0; if(p[2*i+1]>1)p[2*i+1]=1;
    }
    double e_p=energy(p);
    memcpy(X0,p,2*N*sizeof*X0);
    double e_pd=climb(p,budget,&used);

    /* displacement produced by each capped descent, as a fraction of the cap allowed */
    double mv_h=0, mv_s=0;
    for(int i=0;i<N;i++){
        double dx=h[2*i]-X[2*i], dy=h[2*i+1]-X[2*i+1];
        mv_h+=sqrt(dx*dx+dy*dy);
        dx=b[2*i]-s[2*i]; dy=b[2*i+1]-s[2*i+1];
        mv_s+=sqrt(dx*dx+dy*dy);
    }
    mv_h/=(double)N*cap; mv_s/=(double)N*cap;
    printf("%d %d %.8f %.8f %.8f %.8f %.8f %.8f %.6f %.6f\n",
           N,M, e_h,e_hd, e_s,e_sd, e_p,e_pd, mv_h, mv_s);
    return 0;
}
