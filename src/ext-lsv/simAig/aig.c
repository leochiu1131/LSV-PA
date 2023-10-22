#include "base/abc/abc.h"
#include <stdio.h>

int Abc_NtkSimAig(Abc_Ntk_t * pNtk, char* filename){
    
    FILE* file;
    int i,j,retval,count=0;
    char *buf;
    // char* buf=NULL;
    char **output;
    int flag=0;
    int nPIs = Abc_NtkPiNum(pNtk);

    buf = malloc((nPIs+1) * sizeof(char));
    output=malloc(Abc_NtkPoNum(pNtk)*sizeof(char*));
    for(i=0;i<Abc_NtkPoNum(pNtk);i++)
        output[i] = malloc(32*sizeof(char));
    
    file = fopen(filename,"r");
    fseek(file,0L, SEEK_SET);
    if(file == NULL){
        printf("file not found\n");
        return 1;
    }
    // // check format
    // char *firstline = malloc((nPIs) * sizeof(char));
    size_t n;
    // getline(&firstline,&n,file);
    // if(n!=nPIs){
    //     printf("warning n not expected: %ld\n",(n/sizeof(char)));
    //     printf("read %s\n",firstline);
    // }
    // fseek(file,0L, SEEK_SET);

    // while(!feof(file)){
    // // while(retval!=-1 &!feof(file)){
    //     retval = getline(&buf,&n,file);
    //     printf("getline size:%ld %s\n",n,buf);
    // }

    // size_t n=nPIs+1;
    while(!feof(file)){
        // retval = fread(buf,sizeof(char),(nPIs+1),file);
        // printf("buf: %s.\n",buf);
        retval = getline(&buf,&n,file);
        // retval = fgets(buf,n,file);
        // printf("getline size:%ld %s.\n",n,buf);
        // if(retval< (nPIs+1)){
        //     if(feof(file)){
        //         // printf("end of file\n");
        //         // count = -1;
        //         count++;
        //         flag=1;
        //         // count = ((retvalz+1)/(nPIs+1)) * nPIs;
        //     }
        //     else if(ferror(file)){
        //         printf("read error\n");
        //         return 1;
        //     }
        //     else{
        //         printf("unknown read error\n");
        //         return 1;
        //     }
        // }else{
        //     count ++;
        //     // printf("count: %d",count);
        // }
        if(retval){
            count++;
            if(feof(file)){
                flag=1;
            }
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
                // printf("%s:%d\n",Abc_ObjName(pObj),pObj->iTemp);
            }else{
                pObj->iTemp = '1';
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
        char* tmp;
        // print output
        // if(flag){
        if(count==32 || flag){
            // printf("done\n");
            if(count%32==0){
                // for(j=0;j<Abc_NtkPoNum(pNtk);j++){
                //     tmp = malloc(count*2*sizeof(char));
                //     for(i=0;i<count;i++){
                //         tmp[i]=output[j][i];
                //     }
                //     free(output[j]);
                //     output[j]=tmp;
                // }
                Abc_NtkForEachPo(pNtk,pPo,i) {
                    tmp = malloc((count+32)*sizeof(char));
                    for(j=0;j<count;j++){
                        tmp[j]=output[i][j];
                        // if(j==0){
                        //     printf("%s: %c\n",Abc_ObjName(pPo),output[i][0]);
                        // }
                    }
                    free(output[i]);
                    output[i]=tmp;
                }
            // if(count==32){
                // Abc_NtkForEachPo(pNtk,pPo,i) {
                //     // printf("%s: %s\n",Abc_ObjName(pPo),output[i]);
                //     printf("%s:",Abc_ObjName(pPo));
                //     for(j=0;j<32;j++)
                //         printf("%c",output[i][j]);
                //     printf("\n");
                // }
                // count=0;
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
        // free(buf);
        // buf=NULL;

    }
    
    
    // TODO4: clean
    free(buf);
    for(i=0;i<nPIs;i++)
        output = malloc(32*sizeof(char));
    output=malloc(nPIs*sizeof(char*));
    fclose(file);

    return 0;
}
