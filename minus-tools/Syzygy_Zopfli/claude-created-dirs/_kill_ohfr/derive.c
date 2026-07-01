#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t AbsDiff(size_t x, size_t y){ return x>y ? x-y : y-x; }
void OHFR(int length, size_t *counts){
    int i,k,stride; size_t symbol,sum,limit; int *g;
    for(;length>=0;--length){ if(length==0)return; if(counts[length-1]!=0)break; }
    g=(int*)malloc((unsigned)length*sizeof(int));
    for(i=0;i<length;++i)g[i]=0;
    symbol=counts[0];stride=0;
    for(i=0;i<length+1;++i){
        if(i==length||counts[i]!=symbol){
            if((symbol==0&&stride>=5)||(symbol!=0&&stride>=7)){for(k=0;k<stride;++k)g[i-k-1]=1;}
            stride=1; if(i!=length)symbol=counts[i];
        } else ++stride;
    }
    stride=0;limit=counts[0];sum=0;
    for(i=0;i<length+1;++i){
        if(i==length||g[i]||AbsDiff(counts[i],limit)>=4){
            if(stride>=4||(stride>=3&&sum==0)){
                int count=(sum+stride/2)/stride;
                if(count<1)count=1; if(sum==0)count=0;
                for(k=0;k<stride;++k)counts[i-k-1]=count;
            }
            stride=0;sum=0;
            if(i<length-3)limit=(counts[i]+counts[i+1]+counts[i+2]+counts[i+3]+2)/4;
            else if(i<length)limit=counts[i]; else limit=0;
        }
        ++stride; if(i!=length)sum+=counts[i];
    }
    free(g);
}
/* predicted closed form for L=4, c3!=0 */
static int predict(size_t c0,size_t c1,size_t c2,size_t c3,size_t out[4]){
    out[0]=c0;out[1]=c1;out[2]=c2;out[3]=c3;
    if(c3==0) return 1; /* trim cases all identity */
    size_t a=c0>c1?c0-c1:c1-c0, b=c0>c2?c0-c2:c2-c0, d=c0>c3?c0-c3:c3-c0;
    if(a<4&&b<4&&d<4){
        size_t s=c0+c1+c2+c3; long cnt=(long)((s+2)/4); if(cnt<1)cnt=1; if(s==0)cnt=0;
        out[0]=out[1]=out[2]=out[3]=(size_t)cnt;
    }
    return 1;
}
int main(void){
    int bad=0, tot=0;
    int R=14;
    for(int a=0;a<R;a++)for(int b=0;b<R;b++)for(int c=0;c<R;c++)for(int d=0;d<R;d++){
        size_t in[4]={a,b,c,d}, c4[4]; memcpy(c4,in,sizeof c4);
        OHFR(4,c4);
        size_t pr[4]; predict(a,b,c,d,pr);
        tot++;
        if(memcmp(c4,pr,sizeof c4)){ bad++; if(bad<20) printf("MISMATCH in=[%d,%d,%d,%d] got=[%zu,%zu,%zu,%zu] pred=[%zu,%zu,%zu,%zu]\n",a,b,c,d,c4[0],c4[1],c4[2],c4[3],pr[0],pr[1],pr[2],pr[3]); }
    }
    printf("tot=%d bad=%d\n",tot,bad);
    return 0;
}
