#include "base/abc/abc.h"
#include <stdio.h>

int Abc_NtkSimAig(Abc_Ntk_t * pNtk, char* filename){
    
    FILE* file;
    int i,j,retval,count=0;
    char *buf, **output;
    int flag=0;
    int nPIs = Abc_NtkPiNum(pNtk);

    buf = malloc((nPIs+1) * sizeof(char));
    output=malloc(nPIs*sizeof(char*));
    for(i=0;i<nPIs;i++)
        output[i] = malloc(32*sizeof(char));
    
    file = fopen(filename,"r");
    fseek(file,0L, SEEK_SET);
    if(file == NULL){
        printf("file not found\n");
        return 1;
    }

    while(!feof(file)){
    // while(count>=0){
        retval = fread(buf,sizeof(char),(nPIs+1),file);
        if(retval< (nPIs+1)){
            if(feof(file)){
                // printf("end of file\n");
                // count = -1;
                count++;
                flag=1;
                // count = ((retvalz+1)/(nPIs+1)) * nPIs;
            }
            else if(ferror(file)){
                printf("read error\n");
                return 1;
            }
            else{
                printf("unknown read error\n");
                return 1;
            }
        }else{
            count ++;
        }
        if((buf[nPIs]=='1' || buf[nPIs]=='0')){
            printf("warning: input pattern length mismatch\n");
            // printf("warning: count(%d) nPId(%d) buf[nPIs](%d)\n",count,nPIs,(int)buf[nPIs]);
            return 1;
        }
        // printf("count(%d) nPId(%d)\n",count,nPIs);
        // assign primary inputs
        Abc_Obj_t* pPi;
        Abc_NtkForEachPi( pNtk, pPi, i ){
            pPi->iTemp =( buf[i]=='1')?1:0;
            // printf("%s:%d\n",Abc_ObjName(pPi),pPi->iTemp); 
        }
        // calculate for each node
        Abc_Obj_t* pObj;
        int in0, in1;
        Abc_NtkForEachNode( pNtk, pObj, i ) {
            if(Abc_ObjIsCi(pObj)){
                printf("%s:%d\n",Abc_ObjName(pObj),pObj->iTemp);
            }
            else if( !Abc_AigNodeIsConst(pObj) ) {
                in0 = Abc_ObjFaninC0(pObj)?(Abc_ObjFanin0(pObj)->iTemp ^1):(Abc_ObjFanin0(pObj)->iTemp);
                in1 = Abc_ObjFaninC1(pObj)?(Abc_ObjFanin1(pObj)->iTemp ^1):(Abc_ObjFanin1(pObj)->iTemp);
                if( in0 && in1) pObj->iTemp = 1;
                else pObj->iTemp = 0;
            }    
        }
        // TODO3: match primary output with nodes
        Abc_Obj_t* pPo;
        Abc_NtkForEachPo(pNtk,pPo,i) {
            pPo->iTemp = Abc_ObjFaninC0(pPo)?(Abc_ObjFanin0(pPo)->iTemp ^1):(Abc_ObjFanin0(pPo)->iTemp);
            // printf("%s:%d\n",Abc_ObjName(pPo),pPo->iTemp);
            // if(count>=0)
                output[i][count-1] = '0'+pPo->iTemp;
        }
        // print output
        if(count==32 || flag){
            if(count==32){
                Abc_NtkForEachPo(pNtk,pPo,i) {
                    printf("%s: %s\n",Abc_ObjName(pPo),output[i]);
                }
            }else{
                Abc_NtkForEachPo(pNtk,pPo,i) {
                    printf("%s: ",Abc_ObjName(pPo));
                    for(j=0;j<count;j++)
                        printf("%c",output[i][j]);
                        // printf("%c-%d--",output[i][j],j);
                    printf("\n");
                }
            }
        }
    }
    
    
    // TODO4: clean
    free(buf);
    // for(i=0;i<nPIs;i++)
    //     output = malloc(32*sizeof(char));
    // output=malloc(nPIs*sizeof(char*));
    fclose(file);

    return 0;
}