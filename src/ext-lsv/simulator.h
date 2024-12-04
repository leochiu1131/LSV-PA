
typedef struct simCareNode
{
  int nodeId;
  int fanin0Value;
  int fanin1Value;
  int fanin0NodeId;
  int fanin1NodeId;
}simCareNode;


typedef struct simnode
{
  simCareNode *p;
  int8_t nodeValue;
}simNode;




struct Abc_obj_sim
{
  Abc_Obj_t *pObj;
  int sim_value;
};
typedef struct my_aig_node_ my_aig_node;



struct my_aig_node_
{
  Abc_Obj_t *pObj;
  int literal_id;
  int abc_obj_id;
  int node_type;
};









int Simulation(Abc_Ntk_t *pNtk, u_int64_t patterns)
{
  if(!Abc_NtkIsStrash(pNtk))
    pNtk = Abc_NtkStrash(pNtk, 0, 0, 0);
  
  int nodeNumber = Abc_NtkObjNum(pNtk);
  //printf("Node Number: %d\n",nodeNumber);
  std::vector<simNode> simNodeArray(nodeNumber+1);
  Abc_Obj_t *IterateObj;
  int obj_ite_num = 0;


  Abc_NtkForEachObj(pNtk, IterateObj, obj_ite_num)
  {
    int nodeId = Abc_ObjId(IterateObj);
    simNodeArray[nodeId].nodeValue = 2;
    //printf("Node ID: %d Node type: %d\n",nodeId,Abc_ObjType(IterateObj));
    if(Abc_ObjType(IterateObj) == ABC_OBJ_PI)
    {
      int shiftDigit = nodeId - 1;
      bool boolNodeValue = (patterns & (1 << shiftDigit))>>shiftDigit;
      simNodeArray[nodeId].nodeValue = boolNodeValue;
      //printf("\t Node value: %d\n",simNodeArray[nodeId].nodeValue ); 
    }
    if(Abc_ObjType(IterateObj) != ABC_OBJ_NODE)
      continue;
    
    int faninId[2] = {Abc_ObjId(Abc_ObjFanin0(IterateObj)), Abc_ObjId(Abc_ObjFanin1(IterateObj))};
    //printf("Fanin0 ID: %d, Fanin1 ID: %d\n",faninId[0],faninId[1]);
    int faninInv[2] = {Abc_ObjFaninC0(IterateObj), Abc_ObjFaninC1(IterateObj)};
    int faninVal[2] = {(simNodeArray[faninId[0]].nodeValue ^ faninInv[0]),(simNodeArray[faninId[1]].nodeValue ^ faninInv[1])};
    simNodeArray[nodeId].nodeValue = faninVal[0] & faninVal[1];

    
  }
  





  int PoCntIndex = 0;
  int outputSum = 0;
  Abc_NtkForEachPo(pNtk, IterateObj, obj_ite_num)
  {

    int nodeId = Abc_ObjId(IterateObj);
    //printf("Po id :%d",nodeId); 
    int faninId = Abc_ObjId(Abc_ObjFanin0(IterateObj));
    int faninInv = Abc_ObjFaninC0(IterateObj);
    int faninVal = simNodeArray[faninId].nodeValue ^ faninInv;
    simNodeArray[nodeId].nodeValue = faninVal;
    //printf("Po id :%d, Po value: %d\n",nodeId,faninVal);
    outputSum += faninVal<<PoCntIndex++;
    
  }

  
return outputSum;
}


int Simulation(Abc_Ntk_t *pNtk, u_int64_t patterns,int careNode, int *pCareValue)
{
  if(!Abc_NtkIsStrash(pNtk))
    pNtk = Abc_NtkStrash(pNtk, 0, 0, 0);
  
  int nodeNumber = Abc_NtkObjNum(pNtk);
  //printf("Node Number: %d\n",nodeNumber);
  std::vector<simNode> simNodeArray(nodeNumber+1);
  Abc_Obj_t *IterateObj;
  int obj_ite_num = 0;


  Abc_NtkForEachObj(pNtk, IterateObj, obj_ite_num)
  {
    int nodeId = Abc_ObjId(IterateObj);
    simNodeArray[nodeId].nodeValue = 2;
    //printf("Node ID: %d Node type: %d\n",nodeId,Abc_ObjType(IterateObj));
    if(Abc_ObjType(IterateObj) == ABC_OBJ_PI)
    {
      int shiftDigit = nodeId - 1;
      bool boolNodeValue = (patterns & (1 << shiftDigit))>>shiftDigit;
      simNodeArray[nodeId].nodeValue = boolNodeValue;
      //printf("\t Node value: %d\n",simNodeArray[nodeId].nodeValue ); 
    }
    if(Abc_ObjType(IterateObj) != ABC_OBJ_NODE)
      continue;
    
    int faninId[2] = {Abc_ObjId(Abc_ObjFanin0(IterateObj)), Abc_ObjId(Abc_ObjFanin1(IterateObj))};
    //printf("Fanin0 ID: %d, Fanin1 ID: %d\n",faninId[0],faninId[1]);
    int faninInv[2] = {Abc_ObjFaninC0(IterateObj), Abc_ObjFaninC1(IterateObj)};
    int faninVal[2] = {(simNodeArray[faninId[0]].nodeValue ^ faninInv[0]),(simNodeArray[faninId[1]].nodeValue ^ faninInv[1])};
    simNodeArray[nodeId].nodeValue = faninVal[0] & faninVal[1];

    if(careNode == nodeId)
    {
      simCareNode * SimCareNodeData = (simCareNode *)malloc(sizeof(simCareNode));
      SimCareNodeData->nodeId = nodeId;
      SimCareNodeData->fanin0Value = faninVal[0];
      SimCareNodeData->fanin1Value = faninVal[1];
      SimCareNodeData->fanin0NodeId = faninId[0];
      SimCareNodeData->fanin1NodeId = faninId[1];
      simNodeArray[nodeId].p = SimCareNodeData;
      *pCareValue = faninVal[0] + faninVal[1]<<1;
    }
    
  }
  





  int PoCntIndex = 0;
  int outputSum = 0;
  Abc_NtkForEachPo(pNtk, IterateObj, obj_ite_num)
  {

    int nodeId = Abc_ObjId(IterateObj);
    //printf("Po id :%d",nodeId); 
    int faninId = Abc_ObjId(Abc_ObjFanin0(IterateObj));
    int faninInv = Abc_ObjFaninC0(IterateObj);
    int faninVal = simNodeArray[faninId].nodeValue ^ faninInv;
    simNodeArray[nodeId].nodeValue = faninVal;
    //printf("Po id :%d, Po value: %d\n",nodeId,faninVal);
    outputSum += faninVal<<PoCntIndex++;
    
  }

  
return outputSum;
}










#ifdef SIMAIG_RETURN_MAP
std::map<int, Abc_obj_sim *> Lsv_SimAIG(Abc_Ntk_t *pNtk, u_int64_t patterns)
#else
int Lsv_SimAIG(Abc_Ntk_t *pNtk, u_int64_t patterns)
#endif
{
    int simualtion_result = 0;
  // 1. find all pis

  std::map<int, Abc_obj_sim *> obj_sim_map; // first is id, second is obj
  std::map<int, Abc_obj_sim *> output_simMap;
  Abc_Obj_t *pObj;

  int pi_ite_num = 0;
  Abc_NtkForEachPi(pNtk, pObj, pi_ite_num)
  {
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    pobj_sim->pObj = pObj;
    pobj_sim->sim_value = patterns & (1 << pi_ite_num) ? 1 : 0;
    obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
#if PRINT_SIMULATION_RESULT
    printf("------------------------------------\n");
    printf("PI ID: %2d, Name: %5s, Value: %d\n", Abc_ObjId(pobj_sim->pObj), Abc_ObjName(pobj_sim->pObj), pobj_sim->sim_value);
#endif
  }
  // Print obj_sim_map

  int obj_ite_num = 0;
  Abc_NtkForEachNode(pNtk, pObj, obj_ite_num)
  {
    // printf("\tCompute throght ID: %d\n", Abc_ObjId(pObj));
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    pobj_sim->pObj = pObj;

    Abc_Obj_t *pFain;
    int fanin_num = 0;

    bool fanin_compl_edge[2] = {(bool)pObj->fCompl0, (bool)pObj->fCompl1};
    int fanin_id[2] = {0, 0};

    Abc_ObjForEachFanin(pObj, pFain, fanin_num)
    {
      fanin_id[fanin_num] = Abc_ObjId(pFain);
#if PRINT_SIMULATION_PROCESS
      if (fanin_num == 1)
        printf("\t\tNode_ID: %3d, Fanin0_ID: %3d, Fanin1_ID: %3d, Fanin0_Compl: %d Fanin1_Compl: %d \n", Abc_ObjId(pObj), fanin_id[0], fanin_id[1], fanin_compl_edge[0], fanin_compl_edge[1]);
#endif
    }

    // simulation data
    bool edge_0_val = obj_sim_map.find(fanin_id[0])->second->sim_value;
    edge_0_val = fanin_compl_edge[0] ? !edge_0_val : edge_0_val;
    bool edge_1_val = obj_sim_map.find(fanin_id[1])->second->sim_value;
    edge_1_val = fanin_compl_edge[1] ? !edge_1_val : edge_1_val;

    pobj_sim->sim_value = edge_0_val & edge_1_val;
    obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
  }


int fanout_cnt = 0;
  Abc_NtkForEachPo(pNtk, pObj, obj_ite_num)
  {
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    Abc_Obj_t *pFain;
    
    int dummyVar = 0;
    pobj_sim->pObj = pObj;

    Abc_ObjForEachFanin(pObj, pFain, dummyVar)
    {
      bool output_val = pObj->fCompl0 ? !obj_sim_map[Abc_ObjId(pFain)]->sim_value : obj_sim_map[Abc_ObjId(pFain)]->sim_value;
      obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
      pobj_sim->sim_value = output_val;
      output_simMap[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
      simualtion_result += output_val<<fanout_cnt;
#if PRINT_SIMULATION_RESULT
      printf("PO ID: %2d, Name: %5s, Value: %d\n", Abc_ObjId(pobj_sim->pObj), Abc_ObjName(pobj_sim->pObj), pobj_sim->sim_value);
      printf("------------------------------------\n");
#endif
    }
    fanout_cnt++;
  }

// Dump the map
#if PRINT_SIMULATION_MAP
  for (const auto &entry : obj_sim_map)
  {
    printf("\t\tName: %d,\t Obj: %6s\t\t Value:%d\n", entry.first, Abc_ObjName(entry.second->pObj), (entry.second->sim_value));
  }
#endif

#if SIMAIG_RETURN_MAP
    return output_simMap;
#else
    return simualtion_result;
#endif
  
}



#ifdef SIMAIG_RETURN_MAP
std::map<int, Abc_obj_sim *> Lsv_SimAIG(Abc_Ntk_t *pNtk, u_int64_t patterns)
#else
int Lsv_SimAIG(Abc_Ntk_t *pNtk, u_int64_t patterns,int careNode, int *pCareValue)
#endif
{
    int simualtion_result = 0;
  // 1. find all pis

  std::map<int, Abc_obj_sim *> obj_sim_map; // first is id, second is obj
  std::map<int, Abc_obj_sim *> output_simMap;
  Abc_Obj_t *pObj;

  int pi_ite_num = 0;
  Abc_NtkForEachPi(pNtk, pObj, pi_ite_num)
  {
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    pobj_sim->pObj = pObj;
    pobj_sim->sim_value = patterns & (1 << pi_ite_num) ? 1 : 0;
    obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
#if PRINT_SIMULATION_RESULT
    printf("------------------------------------\n");
    printf("PI ID: %2d, Name: %5s, Value: %d\n", Abc_ObjId(pobj_sim->pObj), Abc_ObjName(pobj_sim->pObj), pobj_sim->sim_value);
#endif
  }
  // Print obj_sim_map

  int obj_ite_num = 0;
  Abc_NtkForEachNode(pNtk, pObj, obj_ite_num)
  {
    // printf("\tCompute throght ID: %d\n", Abc_ObjId(pObj));
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    pobj_sim->pObj = pObj;

    Abc_Obj_t *pFain;
    int fanin_num = 0;

    bool fanin_compl_edge[2] = {(bool)pObj->fCompl0, (bool)pObj->fCompl1};
    int fanin_id[2] = {0, 0};

    Abc_ObjForEachFanin(pObj, pFain, fanin_num)
    {
      fanin_id[fanin_num] = Abc_ObjId(pFain);
#if PRINT_SIMULATION_PROCESS
      if (fanin_num == 1)
        printf("\t\tNode_ID: %3d, Fanin0_ID: %3d, Fanin1_ID: %3d, Fanin0_Compl: %d Fanin1_Compl: %d \n", Abc_ObjId(pObj), fanin_id[0], fanin_id[1], fanin_compl_edge[0], fanin_compl_edge[1]);
#endif
    }

    // simulation data
    bool edge_0_val = obj_sim_map.find(fanin_id[0])->second->sim_value;
    edge_0_val = fanin_compl_edge[0] ? !edge_0_val : edge_0_val;
    bool edge_1_val = obj_sim_map.find(fanin_id[1])->second->sim_value;
    edge_1_val = fanin_compl_edge[1] ? !edge_1_val : edge_1_val;

    pobj_sim->sim_value = edge_0_val & edge_1_val;
    obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
  }
int fanout_cnt = 0;
  Abc_NtkForEachPo(pNtk, pObj, obj_ite_num)
  {
    Abc_obj_sim *pobj_sim = new Abc_obj_sim;
    Abc_Obj_t *pFain;
    int dummyVar = 0;
    pobj_sim->pObj = pObj;

    Abc_ObjForEachFanin(pObj, pFain, dummyVar)
    {
      bool output_val = pObj->fCompl0 ? !obj_sim_map[Abc_ObjId(pFain)]->sim_value : obj_sim_map[Abc_ObjId(pFain)]->sim_value;
      obj_sim_map[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
      pobj_sim->sim_value = output_val;
      output_simMap[Abc_ObjId(pobj_sim->pObj)] = pobj_sim;
      simualtion_result += output_val<<fanout_cnt;
#if PRINT_SIMULATION_RESULT
      printf("PO ID: %2d, Name: %5s, Value: %d\n", Abc_ObjId(pobj_sim->pObj), Abc_ObjName(pobj_sim->pObj), pobj_sim->sim_value);
      printf("------------------------------------\n");
#endif
    }
    fanout_cnt++;
  }

// Dump the map
#if PRINT_SIMULATION_MAP
  for (const auto &entry : obj_sim_map)
  {
    printf("\t\tName: %d,\t Obj: %6s\t\t Value:%d\n", entry.first, Abc_ObjName(entry.second->pObj), (entry.second->sim_value));
  }
#endif

  if(careNode != 0){
    std::string nodeNameStr = "n" + std::to_string(careNode);
    char *pNodeName = const_cast<char*>(nodeNameStr.c_str());
    Abc_Obj_t* targetNode = Abc_NtkFindNode(pNtk,pNodeName);
    int simval_fanin0 = (bool)obj_sim_map.find(Abc_ObjId(Abc_ObjFanin0(targetNode)))->second->sim_value ^ (bool)targetNode->fCompl0;
    int simval_fanin1 = (bool)obj_sim_map.find(Abc_ObjId(Abc_ObjFanin1(targetNode)))->second->sim_value ^ (bool)targetNode->fCompl1;
    *pCareValue = simval_fanin0 + simval_fanin1*2;
  }
    


#if SIMAIG_RETURN_MAP
    return output_simMap;
#else
    return simualtion_result;
#endif
  
}



int sat_add_clause(sat_solver *sat, std::map<int, int> clause_map)
{
  int literal_index = 0;
  const int nMap = clause_map.size();
  lit *pNewClause = new lit[clause_map.size()];
  for (auto it = clause_map.rbegin(); it != clause_map.rend(); it++, literal_index++)
  {
    pNewClause[literal_index] = toLitCond((*it).first, (*it).second);

     //char symbol = (*it).second == 1 ? '!' : ' ';
     //printf("pNewClause[%d] = %d  ",literal_index,pNewClause[literal_index]) ;
  }
   //printf("\n");
  int state = sat_solver_addclause(sat, pNewClause, pNewClause + nMap);
  //printf("Add clause state %d\n",state);
  return state;
}


// * Return <literal id,value>
std::map<int, int> getSatResult(sat_solver *sat, std::map<int, my_aig_node> nodeRecordTable)
{
  std::map<int, int> result;
  for (int l = 0; l < sat->size; l++)
  {
    auto it = nodeRecordTable.find(l);
    if (it != nodeRecordTable.end())
    {
      int sat_ans = 0;

      if (it->second.literal_id != -1 && it->second.pObj->Type == ABC_OBJ_PI)
      //if (it->second.literal_id != -1 && 1)
      {
        
        sat_ans = sat_solver_var_value(sat, it->second.literal_id);
         //printf("sat_solver_var_value: %d id:%d \n", sat_solver_var_value(sat, it->second.literal_id),it->second.literal_id);
        // printf("sat_solver_get_var_value: %d\n", sat_solver_get_var_value(sat, it->second.literal_id));
        result[it->second.literal_id] = sat_ans;
        /*
        if (it->second.pObj->Type == ABC_OBJ_PO || it->second.pObj->Type == ABC_OBJ_PI){
          printf("\tLiteral id:%d Object id:%2d Obj name:%4s ans:%d\n", it->second.literal_id, it->second.abc_obj_id, Abc_ObjName(it->second.pObj), sat_ans);
        }
        */
      }
    }
  }
  return result;
}

int simulation(Abc_Ntk_t *pNtk,int simulation_patten)
{
  int simTableSize = Abc_NtkObjNumMax(pNtk);
  std::vector<int> simTable(simTableSize, 8);


  int i;
  Abc_Obj_t *pObj;

  Abc_NtkForEachPi(pNtk, pObj, i)
  {
    simTable[Abc_ObjId(pObj)] = simulation_patten & (1<<Abc_ObjId(pObj)) == 0 ? 0 : 1;
  }

  Abc_NtkForEachPo(pNtk, pObj, i)
  {

  }


}






int testSim(Abc_Ntk_t *pNtk)
{

  int PoNum = Abc_NtkPiNum(pNtk);
  if(PoNum>32)
  {
    printf("Too many PI %d\n",PoNum);
  }

  int error_cnt = 0;
  clock_t  tStart = clock();
  printf("Start simulation\n");
  for (int i = 0; i < pow(2,PoNum); i++)
  {
    //printf("Runing Lsv_SimAIG @%d\n",i);
    long int simValue = Lsv_SimAIG(pNtk, i);
    //printf("Runing Simulation @%d\n",i);
    long int simValue2 = Simulation(pNtk,i);

    if( simValue != simValue2)
    {
      printf("Simulation error on pattern %d\n",i);
      printf("Lsv_SimAIG value: %d\n",simValue);
      printf("Simulation value2: %d\n",simValue2);
      error_cnt++;
    }


    
  }
  printf("Error count: %d\n",error_cnt);
  printf("Simualtion taken: %.8fs\n", (double)(clock() - tStart)/CLOCKS_PER_SEC);

return 0;

  
}