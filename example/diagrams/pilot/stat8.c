/* stationarity pilot: is a human-authored layout a local minimum of a standard aesthetic energy? */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#ifndef ALIGN_DEF
#define ALIGN_DEF 1
#endif
static int N,M,*EA,*EB; static double *W,*H,*X;
static double Wc=1.0,Wl=1.0,Wo=1.0,Wr=0.0,Wa=0.0,Wn=0.0,L,CAP=1e9; static double *X0;
static uint32_t st_;
static uint32_t r32(void){ st_^=st_<<13; st_^=st_>>17; st_^=st_<<5; return st_; }
static double uni(void){ return r32()/4294967296.0; }
static int seg(double ax,double ay,double bx,double by,double cx,double cy,double dx,double dy){
    double d1=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax), d2=(bx-ax)*(dy-ay)-(by-ay)*(dx-ax);
    double d3=(dx-cx)*(ay-cy)-(dy-cy)*(ax-cx), d4=(dx-cx)*(by-cy)-(dy-cy)*(bx-cx);
    return ((d1>0)!=(d2>0)) && ((d3>0)!=(d4>0));
}
static double TC,TL,TO,TR,TA,TN;
static double energy(const double*x){
    double c=0,len=0,ov=0,anode=0,orth=0,align=0,nedge=0; int i,j;
    for(i=0;i<M;i++){
        double ax=x[2*EA[i]],ay=x[2*EA[i]+1],bx=x[2*EB[i]],by=x[2*EB[i]+1];
        double dxe=fabs(bx-ax), dye=fabs(by-ay);
        double d=sqrt(dxe*dxe+dye*dye);
        len+=(d-L)*(d-L)/(L*L);
        /* orthogonality: 0 when the edge is horizontal or vertical, 0.5 at 45 degrees */
        if(dxe+dye>0) orth += (dxe<dye?dxe:dye)/(dxe+dye);
        for(j=i+1;j<M;j++){
            if(EA[i]==EA[j]||EA[i]==EB[j]||EB[i]==EA[j]||EB[i]==EB[j]) continue;
            if(seg(ax,ay,bx,by,x[2*EA[j]],x[2*EA[j]+1],x[2*EB[j]],x[2*EB[j]+1])) c+=1;
        }
        /* node-edge separation: any node that is not an endpoint sitting near this edge */
        for(j=0;j<N;j++){
            if(j==EA[i]||j==EB[i]) continue;
            double px=x[2*j]-ax, py=x[2*j+1]-ay, ex=bx-ax, ey=by-ay;
            double L2=ex*ex+ey*ey; if(L2<=0) continue;
            double tt=(px*ex+py*ey)/L2; if(tt<0)tt=0; if(tt>1)tt=1;
            double qx=px-tt*ex, qy=py-tt*ey, dd=sqrt(qx*qx+qy*qy);
            double r=(W[j]+H[j])/4;
            if(dd<r) nedge += (r-dd)*(r-dd)/(r*r);
        }
    }
    for(i=0;i<N;i++){ anode+=W[i]*H[i];
#if ALIGN_DEF == 1
        /* A1: distance to the nearest node that shares a row OR a column with it */
        { double bx=1e9;
          for(j=0;j<N;j++){
            if(j==i) continue;
            double ax2=fabs(x[2*i]-x[2*j]), ay2=fabs(x[2*i+1]-x[2*j+1]);
            double mn=ax2<ay2?ax2:ay2;
            if(mn<bx) bx=mn;
          }
          if(bx<1e8) align+=bx; }
#elif ALIGN_DEF == 2
        /* A2: rows and columns priced separately, not by whichever is closer */
        { double bxx=1e9, byy=1e9;
          for(j=0;j<N;j++){
            if(j==i) continue;
            double ax2=fabs(x[2*i]-x[2*j]), ay2=fabs(x[2*i+1]-x[2*j+1]);
            if(ax2<bxx) bxx=ax2;
            if(ay2<byy) byy=ay2;
          }
          if(bxx<1e8&&byy<1e8) align+=0.5*(bxx+byy); }
#else
        /* A3: a smooth kernel over all pairs, no nearest-neighbour argmin anywhere */
        { double s=0.02, acc=0;
          for(j=0;j<N;j++){
            if(j==i) continue;
            double u=(x[2*i]-x[2*j])/s, v=(x[2*i+1]-x[2*j+1])/s;
            acc += exp(-u*u) + exp(-v*v);
          }
          align += 1.0/(1.0+acc); }
#endif
        for(j=i+1;j<N;j++){
            double ox=(W[i]+W[j])/2-fabs(x[2*i]-x[2*j]), oy=(H[i]+H[j])/2-fabs(x[2*i+1]-x[2*j+1]);
            if(ox>0&&oy>0) ov+=ox*oy;
        }}
    TC=c/(double)M; TL=len/(double)M; TO=ov/anode;
    TR=orth/(double)M; TA=align/(double)N; TN=nedge/(double)M;
    return Wc*TC + Wl*TL + Wo*TO + Wr*TR + Wa*TA + Wn*TN;
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
/* Batch mode: read K graphs, and for the weights given on the command line report the
 * fraction of the energy that a displacement-capped descent still removes from each human
 * layout. That fraction is the stationarity residual under those weights. Fitting the
 * weights means searching for the w that makes it small; the residual that survives is the
 * part of how people draw that no weighting of these terms can express. */
static int Ns[4096],Ms[4096]; static double *Xs[4096],*Ws[4096],*Hs[4096];
static int *EAs[4096],*EBs[4096]; static int K;

int main(int argc,char**argv)
{
    long budget=atol(argv[1]); unsigned seed=(unsigned)atoi(argv[2]);
    double cap=atof(argv[3]);
    Wc=atof(argv[4]); Wl=atof(argv[5]); Wo=atof(argv[6]);
    if(argc>7) Wr=atof(argv[7]);
    if(argc>8) Wa=atof(argv[8]);
    if(argc>9) Wn=atof(argv[9]);
    if(scanf("%d",&K)!=1) return 1;
    for(int g=0; g<K; g++){
        int n,m; if(scanf("%d %d",&n,&m)!=2) return 1;
        Ns[g]=n; Ms[g]=m;
        Xs[g]=malloc(2*n*sizeof(double)); Ws[g]=malloc(n*sizeof(double));
        Hs[g]=malloc(n*sizeof(double));
        EAs[g]=malloc(m*sizeof(int)); EBs[g]=malloc(m*sizeof(int));
        for(int i=0;i<n;i++)
            if(scanf("%lf %lf %lf %lf",&Xs[g][2*i],&Xs[g][2*i+1],&Ws[g][i],&Hs[g][i])!=4) return 1;
        for(int i=0;i<m;i++) if(scanf("%d %d",&EAs[g][i],&EBs[g][i])!=2) return 1;
    }
    st_=seed?seed:1;
    for(int g=0; g<K; g++){
        N=Ns[g]; M=Ms[g]; W=Ws[g]; H=Hs[g]; EA=EAs[g]; EB=EBs[g];
        double *d=malloc(M*sizeof*d);
        for(int i=0;i<M;i++){
            double dx=Xs[g][2*EA[i]]-Xs[g][2*EB[i]], dy=Xs[g][2*EA[i]+1]-Xs[g][2*EB[i]+1];
            d[i]=sqrt(dx*dx+dy*dy);
        }
        for(int i=0;i<M;i++) for(int j=i+1;j<M;j++)
            if(d[j]<d[i]){double t=d[i];d[i]=d[j];d[j]=t;}
        L = d[M/2]>0 ? d[M/2] : 1.0/sqrt((double)N);
        free(d);
        double *h=malloc(2*N*sizeof*h); memcpy(h,Xs[g],2*N*sizeof*h);
        double e_h=energy(h), sc=TC, sl=TL, so=TO;
        CAP=cap; X0=malloc(2*N*sizeof*X0); memcpy(X0,Xs[g],2*N*sizeof*X0);
        long used; double e_hd=climb(h,budget,&used);
        /* How far the descent actually moved the layout, as a fraction of the cap it was
         * allowed. Zero if the human layout is stationary, one if every node ran to the
         * limit. Unlike a fraction of the energy, this cannot be made small by putting the
         * weight on a term that cannot be reduced. */
        double mv=0; for(int i=0;i<N;i++){
            double dx=h[2*i]-Xs[g][2*i], dy=h[2*i+1]-Xs[g][2*i+1];
            mv+=sqrt(dx*dx+dy*dy);
        }
        mv/=(double)N*cap;
        printf("%d %.8f %.8f %.8f %.8f %.8f %.8f\n", N, e_h, e_hd, sc, sl, so, mv);
        free(h); free(X0); X0=NULL;
    }
    return 0;
}
