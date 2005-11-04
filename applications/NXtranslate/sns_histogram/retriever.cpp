#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iterator>
#include <stdlib.h>
#include "retriever.h"
#include "../node.h"
#include "../node_util.h"
#include "../string_util.h"
#include "../tree.hh"
#include "string_location_format.cpp"
#include <time.h>

//#include "test_retriever.cpp"

//#define RETRIEVER_TEST                    //to test main part     
#define RETRIEVER_DECLARATION_TEST        //to test declaration part
#define RETRIEVER_DEFINITION_TEST         //to test definition part
#define RETRIEVER_ARRAY_TEST              //to test allocation of memory of arrays
#define RETRIEVER_INPUT_TEST              //to test the data read
//#define RETRIEVER_MAKE_ARRAY              //to test the process of making the arrays
//#define RETRIEVER_MAKE_ARRAY_PIXELID      //to test the pixelID part
//#define RETRIEVER_MAKE_ARRAY_PIXELID_LIST //to get a listing of the array produced
//#define RETRIEVER_EVERYTHING              //when we don't have everything in the definition part
//#define RETRIEVER_MAKE_ARRAY_PIXELX       //to test the pixelX part
//#define RETRIEVER_MAKE_ARRAY_PIXELX_LIST  //to get a listing of the array produced
//#define RETRIEVER_MAKE_ARRAY_PIXELY       //to test the pixelY part
//#define RETRIEVER_MAKE_ARRAY_PIXELY_LIST  //to get a listing of the array produced
//#define RETRIEVER_MAKE_ARRAY_TBIN         //to test the Tbin part
//#define RETRIEVER_MAKE_ARRAY_TBIN_LIST    //to get a listing of the array produced
//#define RETRIEVER_MAKE_ARRAYS_LIST        //to get a listing of all the arrays made
#define RETRIEVER_MAKE_PRIORITIES         //to test the priority part
//#define RETRIEVER_MAKE_CALCULATION_AND   //to test the calculation of the arrays (and)
//#define RETRIEVER_MAKE_CALCULATION_OR     //to test the calculation of the arrays (or)
//#define RETRIEVER_MAKE_CALCULATION        //to test the Calculation of the array
//#define RETRIEVER_FINAL_RESULT            //to list the final array produced
//#define SWAP_ENDIAN                       //triger the swapping endian subroutine
//#define OUTPUT_RESULT                     //to create binary file of result (to test it with IDL)

const char OutputFileName[]="output_array_binary.dat";

using std::ifstream;
using std::invalid_argument;
using std::runtime_error;
using std::string;
using std::cout;
using std::endl;
using std::vector;
   
//Global variable
vector<string> Tag, Def;
vector<string> VecStr;
vector<int> LocalArray, GlobalArray;  //parameters of the declaration part

//Type of binary file
typedef int binary_type;

struct Grp_parameters   //parameters of the different definitions
{
  int init, last, increment;  //with loop(init,end,increment)
  vector<int> value;          //(value[0],value[1],....)
  char c;                     //c=l for loop and c=p for (x,y,z,...)
};

vector<Grp_parameters> GrpPara;

//Declaration of functions
void DefinitionParametersFunction(vector<string> Def,int OperatorNumber);
void InitLastIncre (string& def, int i);   //Isolate loop(init,last,increment)
void ParseGrp_Value (string& def, int i);  //Isolate values of (....)
void ParseDeclarationArray(vector<string>& LocGlobArray);  //Parse Local and Global array from the declaration part
void CalculateArray (vector<int>& GrpPriority, vector<int>& InverseDef, binary_type* BinaryArray, vector<string> Ope, vector<int>& OpePriority,tree<Node> &tr);  //calculate the final array according to definiton
int FindMaxPriority (vector<int>& GrpPriority);  //find highest priority of Grp
void MakeArray_pixelID (binary_type* MyGrpArray, binary_type* BinaryArray,int grp_number,int InverseDef);  //make pixelID array
void MakeArray_pixelX (binary_type* MyGrpArray, binary_type* BinaryArray,int grp_number,int InverseDef);   //make pixelX array
void MakeArray_pixelY (binary_type* MyGrpArray, binary_type* BinaryArray,int grp_number,int InverseDef);   //make pixelY array
void MakeArray_Tbin (binary_type* MyGrpArray, binary_type* BinaryArray,int grp_number,int InverseDef);     //make Tbin array
void MakeArray_Everything (binary_type* MyGrpArray, binary_type* BinaryArray); //make a copy of the binary array
void DoCalculation (binary_type* GrpArray1, binary_type* GrpArray2,string Operator);  //Do the calculation between the two Arrays
inline void endian_swap(binary_type& x);  //to swap from little to big endian


/*********************************
/SnsHistogramRetriever constructor
`/*********************************/
SnsHistogramRetriever::SnsHistogramRetriever(const string &str): source(str) 
{
  // open the file
  BinaryFile=fopen(source.c_str(),"rb");

  // check that open was successful
  if (BinaryFile==NULL)
    throw invalid_argument("Could not open file: "+source);
}

/*********************************
/SnsHistogramRetriever destructor
/*********************************/
SnsHistogramRetriever::~SnsHistogramRetriever()
{
  // close the file
  if(BinaryFile)
    fclose(BinaryFile);
}

/*********.***********************
/* This is the method for retrieving data from a file. 
/* Interpreting the string is left up to the implementing code.
/********************************/

void SnsHistogramRetriever::getData(const string &location, tree<Node> &tr)
{
  string new_location;
  string DefinitionGrpVersion;    //use to determine priorities
  vector<string> DeclaDef;        //declaration and definition parts
  vector<string> LocGlobArray;    //local and global array (declaration part)
  vector<string> Ope;             //Operators of the defintion part
  int OperatorNumber; 
  string DefinitionPart;          //use to determine the operators
  vector<int> GrpPriority;        //Vector of priority of each group
  vector<int> OpePriority;        //Vector of priority for each operator
  vector<int> InverseDef;        //True=Inverse definition, False=keep it like it is
  int Everything = 0;            //0= we don't want everything, 1=we do
  int GlobalArraySize = 1;       //Size of global array within our program
 
  // check that the argument is not an empty string
  if(location.size()<=0)
    throw invalid_argument("cannot parse empty string");

#ifdef RETRIEVER_TEST
  cout << "####RETRIEVER_TEST#########################################################" << endl;
  cout << "Initial string location= " << endl << "  " << new_location << endl;
#endif

  //Once upon a time, there was a string location named "location"
  //"location" became new_location after loosing its white spaces
  format_string_location(location, new_location);

#ifdef RETRIEVER_TEST
  cout << endl << "String location without white spaces: " << endl << "  " << new_location <<endl;
#endif
  
  //location decided to separate its declaration part (left side of the "#")
  //from its definition part --> DeclaDef vector

  DeclaDef_separator(new_location,DeclaDef);

  if (DeclaDef[1].size() > 0 && DeclaDef[1].size()<7)
    throw runtime_error("Definition part is not valid");

 //check if we want everything
  if (DeclaDef[1] == "")
      {
	Everything = 1;
	Tag.push_back("*");
	Ope.push_back("*");
      }

#ifdef RETRIEVER_EVERYTHING
  if (DeclaDef[1] == "")
    {
      cout << "****RETRIEVER_EVERYTHING*****************************"<<endl;
      cout << "Tag[0]= " << Tag[0]<<endl;
    }
#endif

  //Separate declaration arrays (local and global)
  Declaration_separator(DeclaDef[0], LocGlobArray);

  if (Everything == 0)
    {
     //Work on definition part
     DefinitionPart = DeclaDef[1];
     TagDef_separator(DeclaDef[1],Tag, Def, DefinitionGrpVersion);

#ifdef RETRIEVER_TEST
  cout << endl << "Version Grp of string location: " << endl << "   DefinitionGrpVersion= " << DefinitionGrpVersion << endl;   
#endif

  if (Tag.size()<1)
    throw runtime_error("Definition part is not valid");

#ifdef RETRIEVER_DEFINITION_TEST
 cout << endl << "****RETRIEVER_DEFINITION_TEST********************************************" << endl;
 cout << "List of Tags:" << endl;
 for (int i=0; i<Tag.size();i++)
   cout << "   Tag[" << i << "]= " << Tag[i];
#endif

     //Store operators
     OperatorNumber = Tag.size();
     StoreOperators(DefinitionPart,OperatorNumber, Ope);

     //Give to each grp its priority
     GivePriorityToGrp(DefinitionGrpVersion, OperatorNumber, GrpPriority, InverseDef, OpePriority);   
     //Store parameters of the definition part into GrpPara[0], GrpPara[1]...
     DefinitionParametersFunction(Def,OperatorNumber);
    }
   
 //parse Local and Global Array from Declaration part
  ParseDeclarationArray(LocGlobArray);

  //allocate memory for the binary Array
  for (int i=0; i<GlobalArray.size(); i++)
    {
      GlobalArraySize *= GlobalArray[i];
    }
  binary_type * BinaryArray = new binary_type [GlobalArraySize];
  
  //transfer the data from the binary file into the GlobalArray
  fread(&BinaryArray[0],sizeof(BinaryArray[0]),GlobalArraySize,BinaryFile);

#ifdef RETRIEVER_INPUT_TEST
  cout << endl << endl << "****RETRIEVER_INPUT_TEST********************************************" << endl;
   cout << "Check 10 data of file (before swapping endian, if any):" << endl;

   for (int j=0; j<10; j++)
     {
       cout << "   BinaryArray["<<j<<"]= "<<BinaryArray[j]<<endl;
     }
#endif

#ifdef SWAP_ENDIAN
   for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)
     {
       endian_swap(BinaryArray[j]);
     } 
    
#ifdef RETRIEVER_INPUT_TEST
  cout << endl << endl << "****RETRIEVER_INPUT_TEST********************************************" << endl;
   cout << "Check 10 data of file (after swapping endian):" << endl;

   for (int j=0; j<10; j++)
     {
       cout << "   BinaryArray["<<j<<"]= "<<BinaryArray[j]<<endl;
     }
#endif
#endif

  //Calculate arrays according to definition
  CalculateArray(GrpPriority, InverseDef, BinaryArray, Ope, OpePriority, tr);

cout << endl << "++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
 cout << "Time it took to run it is: " << clock() <<endl;  //REMOVE

}

/*********************************
/SnsHistogramRetriever::MIME_TYPE 
/*********************************/
const string SnsHistogramRetriever::MIME_TYPE("application/x-SNS-histogram");

string SnsHistogramRetriever::toString() const
{
  return "["+MIME_TYPE+"] "+source;
}

/*********************************
/Store values of the definition
/*********************************/
void DefinitionParametersFunction(vector<string> Def,int HowManyDef)
{
  Grp_parameters record;
  string NewString;

#ifdef RETRIEVER_DEFINITION_TEST
 cout << endl << "****RETRIEVER_DEFINITION_TEST********************************************" << endl;
 cout << "List of Definition: " << endl; 
 for (int i =0; i<HowManyDef;i++)
    {
      cout << "   Def["<<i<<"]= " << Def[i]<<endl;
    }
#endif  

  //find out first if it's a loop or a (....)
  for (int i =0; i<HowManyDef;i++)
    {
      if (Def[i].find("loop") < Def[i].size()) 
	{
        record.c = 'l';
        }
      else
	{
        record.c = 'p';
        }
      GrpPara.push_back(record);
    }
 
  //isolate the variable
  for (int i=0; i<HowManyDef;i++)
    {
      if (GrpPara[i].c == 'l')      //loop
	{
	  InitLastIncre(Def[i],i);
	}
      else                          //(....)
	{
	  ParseGrp_Value(Def[i],i);
	}
    }
 return;
}

/*********************************
/Store Initial, last and increment
/*********************************/
void InitLastIncre (string& def, int i)
{
  static const string sep=",";
  int pos1, pos2;
  string new_def;

  //Remove "loop(" and ")"
  def=def.substr(5,def.size()-6);

  //store the info into GrpPara[i].init, end and increment
  pos1 = def.find(sep);
  new_def = def.substr(pos1+1,def.size()-pos1);
  pos2 = new_def.find(sep);
 
  GrpPara[i].init =atoi((def.substr(0,pos1)).c_str());
  GrpPara[i].last =atoi((def.substr(pos1+1,pos2).c_str()));
  GrpPara[i].increment = atoi((new_def.substr(pos2+1, new_def.size()-1).c_str()));
  
#ifdef RETRIEVER_DEFINITION_TEST
  cout << endl << "Def["<<i<<"]: list of values from loop: "<< endl;
  cout << "   GrpPara["<<i<<"].init= "<< GrpPara[i].init;
  cout << "   GrpPara["<<i<<"].last= " << GrpPara[i].last << "   GrpPara["<<i<<"].increment= " << GrpPara[i].increment<<endl;
#endif
  return;
}

/*********************************
/Store values of (......) 
/*********************************/
void ParseGrp_Value(string& def, int i)
{
  int b=0, a=0;

  while (b <= def.size())
    {
      if (def[b]==',')
	{
	  GrpPara[i].value.push_back(atoi((def.substr(a,b-a)).c_str()));
	  a=b+1;
	}
      if (b==def.size())
	{
	  GrpPara[i].value.push_back(atoi((def.substr(a,b-a)).c_str()));
	}
      ++b;
    }

#ifdef RETRIEVER_DEFINITION_TEST
  cout << endl << "Def["<<i<<"], list of values from list of identifiers: " << endl;

   for (int j=0; j<GrpPara[i].value.size(); j++)
    {
      cout << "   GrpPara.value["<<i<<"]= " << GrpPara[i].value[j]<<endl;
    }
#endif

  return;
}

/*********************************
/Parse Local and Global array
/*********************************/


void ParseDeclarationArray(vector<string>& LocGlobArray)
{
  int a=0, b=0;
  

  //Parse Local array
  int i=0;

      //remove square braces
  LocGlobArray[i]=LocGlobArray[i].substr(1,LocGlobArray[i].size()-2);

#ifdef RETRIEVER_DECLARATION_TEST
 cout << endl << "****RETRIEVER_DECLARATION_TEST********************************************" << endl;
 cout << "Local and Global Array: " << endl;
 cout << "   LocGlobArray[" << i << "]= " << LocGlobArray[i]<<endl;   
#endif

  while (b <= LocGlobArray[i].size())
    {
      if (LocGlobArray[i][b]==',')
	{
	  LocalArray.push_back(atoi((LocGlobArray[i].substr(a,b-a)).c_str()));
          a=b+1;}
	
      if (b==LocGlobArray[i].size())
	{
	  LocalArray.push_back(atoi((LocGlobArray[i].substr(a,b-a)).c_str()));
        }
	
      ++b;
    }

  i=1,a=0,b=0;
  
//Parse Global Array

 //remove square braces
  LocGlobArray[i]=LocGlobArray[i].substr(1,LocGlobArray[i].size()-2);
  
#ifdef RETRIEVER_DECLARATION_TEST
     cout << "   LocGlobArray[" << i << "]= " << LocGlobArray[i]<<endl;   
#endif

  while (b <= LocGlobArray[i].size())
    {
      if (LocGlobArray[i][b]==',')
	{
	  GlobalArray.push_back(atoi((LocGlobArray[i].substr(a,b-a)).c_str()));
          a=b+1;}
	
      if (b==LocGlobArray[i].size())
	{
	  GlobalArray.push_back(atoi((LocGlobArray[i].substr(a,b-a)).c_str()));
        }
	
      ++b;
    }
#ifdef RETRIEVER_DECLARATION_TEST
  cout << endl << "Parsing of values: " << endl;
  cout << "   Local array" << endl;

  for (int j=0; j<LocalArray.size();j++)
    {
      cout << "      LocalArray["<<j<<"]= "<<LocalArray[j];
    }

  cout << endl <<"   Global array" << endl;
  
  for (int k=0; k<GlobalArray.size();++k)
    {
      cout << "      GlobalArray["<<k<<"]= "<<GlobalArray[k];
    }

#endif

 return;
}


/*******************************************
/Calculate arrays according to defintion
/*******************************************/
 void CalculateArray (vector<int>& GrpPriority, vector<int>& InverseDef, binary_type* BinaryArray, vector<string> Ope, vector<int>& OpePriority, tree<Node> &tr)
{
  int HighestPriority;
  int GrpNumber = GrpPriority.size();
  int ArraySize = 1;
  int ArraySizeGlobal = 1;
 
  if (Tag[0]!="*")
    {
    HighestPriority = FindMaxPriority(GrpPriority);
    }
 
  //determine array size
  for (int i=0; i<LocalArray.size();i++)
    {
      ArraySize *= LocalArray[i];
    }
 
  if (Tag[0] == "*")
    {
      GrpNumber=1;
      GrpPriority.push_back(1);
    }
  
  //determine array size
  for (int i=0; i<GlobalArray.size();i++)
    {
      ArraySizeGlobal *= GlobalArray[i];
    }
  
  //Allocate memory for each grp
  binary_type **MyGrpArray;
  MyGrpArray = new binary_type*[GrpPriority.size()];

  for (int i=0; i<GrpPriority.size();++i)
    {
      MyGrpArray[i]=new binary_type[ArraySizeGlobal];
    }
  //Allocate memory for the final array created
  binary_type * NewArray = new binary_type [ArraySizeGlobal];   
  
  // binary_type  *MyGrpArray[] = new binary_type[ArraySizeGlobal*GrpPriority.size()];

  //make an array for each group
  for (int i=0; i<GrpPriority.size();++i)
    {

#ifdef RETRIEVER_MAKE_ARRAY
      cout << endl << "****RETRIEVER_MAKE_ARRAY********************************************"<<endl;
      cout << endl << "For Grp # " << i << " , the parameters are:"<<endl;   
      cout << "  Tag= " << Tag[i]<<endl;
      if (Ope.size()!= NULL && i<Ope.size())
	{
	  cout << "  Operator between Grp # "<<i<<" and Grp # "<<i+1<<" : " << Ope[i]<<endl;
	}
#endif        

      if (Tag[i]=="pixelID") 
	{

      MakeArray_pixelID(MyGrpArray[i],BinaryArray,i,InverseDef[i]);


#ifdef RETRIEVER_MAKE_ARRAY_PIXELID_LIST
	  cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELID_LIST*******************************"<<endl;;
	  cout << endl << "Check if MyGrpArray["<<i<<"] in CalculateArray is correct: " << endl;

	  int ll = 0;
	  int mm = 0;
	  cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)  
	    {
	      cout << MyGrpArray[i][j] << " ";            //listing of MyGrpArray    
      
	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl << "   ";
		  mm = 0;
		}
	    } 
#endif
	}
      else if (Tag[i]=="pixelX")
	{
	  MakeArray_pixelX(MyGrpArray[i],BinaryArray,i,InverseDef[i]);


#ifdef RETRIEVER_MAKE_ARRAY_PIXELX_LIST

	  cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELX_LIST****************************************"<<endl;
	  cout << endl << "Check if MyGrpArray["<<i<<"] after MakeArray_pixelX in CalculateArray is correct: " << endl;

	  int ll = 0;
	  int mm = 0;
          cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)
	    {
	      cout << MyGrpArray[i][j] << " ";            //listing of MyGrpArray    
      
	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl;
		  cout << "   ";
		  mm = 0;
		}
	    } 

#endif
	}
      else if (Tag[i]=="pixelY")
	{
#ifdef RETRIEVER_MAKE_ARRAY_PIXELY_LIST

	  cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELY_LIST****************************************"<<endl;
	  cout << endl << "Check if MyGrpArray["<<i<<"] before  MakeArray_pixelY in CalculateArray is correct: " << endl;

	  int ll = 0;
	  int mm = 0;
          cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)
	    {
	      cout << MyGrpArray[i][j] << " ";            //listing of MyGrpArray    
      
	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl;
		  cout << "   ";
		  mm = 0;
		}
	    } 
#endif
	  MakeArray_pixelY(MyGrpArray[i],BinaryArray,i,InverseDef[i]);

#ifdef RETRIEVER_MAKE_ARRAY_PIXELY_LIST

	  cout << "****RETRIEVER_MAKE_ARRAY_PIXELY_LIST****************************************"<<endl;
	  cout << endl << "Check if MyGrpArray["<<i<<"] after MakeArray_pixelY in CalculateArray is correct: " << endl;

	  ll = 0;
	  mm = 0;
          cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)
	    {
	      cout << MyGrpArray[i][j] << " ";            //listing of MyGrpArray    
      
	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl;
		  cout << "   ";
		  mm = 0;
		}
	    } 
#endif
	}
      else if (Tag[i]=="Tbin")
	{
	  MakeArray_Tbin(MyGrpArray[i],BinaryArray,i,InverseDef[i]);

#ifdef RETRIEVER_MAKE_ARRAY_TBIN_LIST

	  cout << endl << "****RETRIEVER_MAKE_ARRAY_TBIN_LIST****************************************"<<endl;
	  cout << endl << "Check if MyGrpArray["<<i<<"] after MakeArray_Tbin in CalculateArray is correct: " << endl;

	  int ll = 0;
	  int mm = 0;
          cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)
	    {
	      cout << MyGrpArray[i][j] << " ";            //listing of MyGrpArray    
      
	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl;
		  cout << "   ";
		  mm = 0;
		}
	    } 
#endif
	}
      else if (Tag[i]=="*")
	{
	  MakeArray_Everything(MyGrpArray[i],BinaryArray);

#ifdef RETRIEVER_EVERYTHING
	  cout << endl << "****RETRIEVER_EVERYTHING****************************************";
	  cout << endl << "Check if MyGrpArray["<<i<<"] in CalculateArray is correct: " << endl<<endl;

	  int ll = 0;
	  int mm = 0;
          cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)
	    {
	      cout << MyGrpArray[i][j] << " ";            //listing of MyGrpArray    
      
	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl;
		  cout << "   ";
		  mm = 0;
		}
	    } 
#endif
	}
}

  //free memory of binary array 
  delete[] BinaryArray;

#ifdef RETRIEVER_MAKE_ARRAYS_LIST
  
  cout << endl<<endl<<"****RETRIEVER_MAKE_ARRAYS_LIST******************************************"<<endl;
  cout << "List all the arrays made"<<endl<<endl;

  cout << "GlobalArray[0]= " << GlobalArray[0]<<endl;
  cout << "GlobalArray[1]= " << GlobalArray[1]<<endl;
  cout << "GlobalArray[2]= " << GlobalArray[2]<<endl<<endl;

for (int i=0; i<GrpPriority.size();++i)
       {
	 //	 plot_array(GlobalArray[0], GlobalArray[1], GlobalArray[2], i, "MyGrpArray", MyGrpArray);

	   for (int i=0; i<GrpPriority.size();++i)
       {
      cout<<"MyGrpArray["<<i<<"]:"<<endl;

      for (int a=0; a<GlobalArray[1];++a)
	{
	  for (int b=0; b<GlobalArray[0];++b)
	    {
	      for (int c=0; c<GlobalArray[2];++c)
		{
		  cout << MyGrpArray[i][c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		  
		  cout << " ";
		}
	      cout << "\t";
	    }
	  cout << endl;
	}
      cout << endl;
	 
      }

#endif
  
  int CurrentPriority = HighestPriority;    
  int GrpPriority_size = GrpPriority.size();   //number of grp

#ifdef RETRIEVER_MAKE_PRIORITIES
      cout << endl << "****RETRIEVER_MAKE_PRIORITIES***************************"<<endl;

      for (int j=0; j<GrpPriority_size;j++)
	{
	  if (GrpPriority[j]>-1)
	    {
	      cout << "   Grp["<<j<<"]/Pri#"<<GrpPriority[j];
	    }
	  else
	    {
	      cout << "   Grp["<<j<<"]/Pri#"<<GrpPriority[j]+1<<" ==>Done!"<<endl;
	    }
	}

      cout << endl;

#endif

  while (GrpPriority_size > 1)     //as long as we have more than just one array
    {for (int i=0; i<GrpPriority_size;++i)   
      {
       //find position of first group with the highest priority
	  if (GrpPriority[i] == CurrentPriority)
	    {//check if it's the last one
	      if (i == GrpPriority_size-1)
		{
		  --GrpPriority[i];
		  break;   //exit the for loop
		}
	      else
		{
		  //check if the next operator has the same priority
		      if ((OpePriority[i] == GrpPriority[i]) && (GrpPriority[i+1] == GrpPriority[i]))
		       {//do calculation according to Ope[i]

#ifdef RETRIEVER_MAKE_PRIORITIES
		      cout << endl << "****RETRIEVER_MAKE_PRIORITIES***************************"<<endl;
		      cout << "Between grp#"<<i<<" and grp#"<<i+1<<": "<<Ope[i]<<" (priority:"<<GrpPriority[i]<<")"<<endl;
#endif
		      DoCalculation(MyGrpArray[i],MyGrpArray[i+1],Ope[i]);

#ifdef RETRIEVER_MAKE_CALCULATION

      cout << "  #### Within Main part of program ####"<<endl<<endl;
      cout << "    MyGrpArray["<<i<<"]:"<<endl;
  
  for (int a=0; a<GlobalArray[1];++a)
	{for (int b=0; b<GlobalArray[0];++b)
	    {for (int c=0; c<GlobalArray[2];++c)
		{cout << MyGrpArray[i][c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		 cout << " ";}
	     cout << "\t";}
	cout << endl;}
  cout << endl;
   
#endif
                       if (i < GrpPriority_size-2)
			 {  
			 //shift to the left the rest of the GrpPriorities
			  for (int k=i+1;k<GrpPriority_size-1;++k)
			    {
			      GrpPriority[k]=GrpPriority[k+1];
			    }

			//shift to the left the rest of the operators and operators priorities
			  for (int j=i;j<GrpPriority_size-2;++j)
			    {
			      OpePriority[j]=OpePriority[j+1];
			      Ope[j]=Ope[j+1];
			    }

			//shift to the left the rest of the arrays
			  for (int k=i+1;k<GrpPriority_size-1;++k)
			    {
			      MyGrpArray[k]=MyGrpArray[k+1];
			    }
			  //free memory of the last array 
			  //  delete[] MyGrpArray[GrpPriority_size-1];
			}
		      
		      //we have one less array/grp
		      --GrpPriority_size; 
		      --i;
		      }
		  else
		    {
		      --GrpPriority[i];   
		    }
		}
	     }
	}

      //check what is the highest priority
      int find_one = 0;
      for (int m=0; m<GrpPriority_size;++m)
	{
	  if (GrpPriority[m] >= CurrentPriority)
	    {
	      find_one = 1;
	      break;
	    }
	}
      if (find_one == 0)
	{
           --CurrentPriority;
	}
    }
  
  /*
  int rank=1;
  // int type=5;    //INT32=NX_INT32
  int type=NX_INT32;
  int dims[NX_MAXRANK];
  void * value;
  cout << "Ok1" <<endl;
  value = (void *)MyGrpArray[0];
  cout << "ok2" << endl;
  Node node("New Array according to string location definition",value,rank,dims,type);
  cout << "ok3" << endl;
  tr.insert(tr.begin(),node);
  cout << "Ok4"<<endl;
  */

  NewArray = MyGrpArray[0];

#ifdef OUTPUT_RESULT
  
#ifdef RETRIEVER_INPUT_TEST
  cout << endl << endl << "****RETRIEVER_INPUT_TEST********************************************" << endl;
   cout << "Check 10 data of file (after swapping endian):" << endl;

   for (int j=0; j<10; j++)
     {
       cout << "   NewArray["<<j<<"]= "<<NewArray[j]<<endl;
     }
#endif    //end of RETRIEVER_INPUT_TEST

  //std::ofstream file2("output_array_binary.dat",std::ios::binary);
  std::ofstream file2(OutputFileName,std::ios::binary);
  file2.write((char *)NewArray,sizeof(NewArray)*ArraySizeGlobal);
  file2.close();

#endif  //end of OUTPUT_RESULT

#ifdef RETRIEVER_FINAL_RESULT
	  cout << endl << "****RETRIEVER_FINAL_RESULT****************************************";
	  cout << endl << "Check the final NewArray: " << endl<<endl;

	  int ll = 0;
	  int mm = 0;
          cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)
	    {
	      cout << NewArray[j] << " ";            //listing of NewArray    

	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";  
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl;    
		  cout << "   ";
		  mm = 0;
		}
	    } 

#endif
  
   delete[] MyGrpArray;  

   return;  
}

/*******************************************`<
/Find highest priority
/*******************************************/
int FindMaxPriority (vector<int>& GrpPriority)
{
  int MaxValue = 0;
  
  for (int i=0; i<GrpPriority.size();i++)
    {
      if (GrpPriority[i]>MaxValue)
	MaxValue = GrpPriority[i];
    }

  return MaxValue;
}

/*******************************************
/Make pixelID array
/*******************************************/
void MakeArray_pixelID (binary_type* MyGrpArray, binary_type* BinaryArray, int grp_number,int InverseDef)
{
  string loop="loop";

if (Def[grp_number][0] == loop[0]) 
 {
#ifdef RETRIEVER_MAKE_ARRAY_PIXELID
      cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELID***************************************" << endl;
      cout << "  Def["<<grp_number<<"]= "<<Def[grp_number]<<endl;
      cout << "  GrpPara["<<grp_number<<"].init= "<<GrpPara[grp_number].init<<endl;
      cout << "  GrpPara["<<grp_number<<"].last= "<<GrpPara[grp_number].last<<endl; 
      cout << "  GrpPara["<<grp_number<<"].increment= "<<GrpPara[grp_number].increment<<endl;
#endif

      if (InverseDef==1)    //case inverse
     {

	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }
      
       for (int i=GrpPara[grp_number].init;i<=GrpPara[grp_number].last;i=i+GrpPara[grp_number].increment)
        {
          for (int tbin=0; tbin < GlobalArray[2];tbin++)
	    {
            MyGrpArray[i*GlobalArray[2]+tbin]=0;
	    }
        }
     }
      else   //normal case
     {
        for (int i=GrpPara[grp_number].init;i<=GrpPara[grp_number].last;i=i+GrpPara[grp_number].increment)
        {
          for (int tbin=0; tbin < GlobalArray[2];tbin++)
	    {
            MyGrpArray[i*GlobalArray[2]+tbin]=BinaryArray[i*GlobalArray[2]+tbin];
	    }
        }
     }
 }
 else   //case with list of identifiers
 {
      if (InverseDef==1)   //case inverse
	{
	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }

	  for (int j=0; j<GrpPara[grp_number].value.size();j++)
	   {
	   for (int tbin=0; tbin < GlobalArray[2]; tbin++)
	     {
	       MyGrpArray[GrpPara[grp_number].value[j]*GlobalArray[2]+tbin]=0;
	     }
	   }
	}
      else
	{
         for (int j=0; j<GrpPara[grp_number].value.size();j++)
	   {
	   for (int tbin=0; tbin < GlobalArray[2]; tbin++)
	     {
	     MyGrpArray[GrpPara[grp_number].value[j]*GlobalArray[2]+tbin]=BinaryArray[GrpPara[grp_number].value[j]*GlobalArray[2]+tbin];
             }
	   }
	}
 }

 
#ifdef RETRIEVER_MAKE_ARRAY_PIXELID_LIST
	  cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELID_LIST*******************************"<<endl;;
	  cout << endl << "Check if MyGrpArray in Make_PixelID is correct: " << endl;

	  int ll = 0;
	  int mm = 0;
	  cout << "   ";

	  for (int j=0; j<GlobalArray[1]*GlobalArray[2]*GlobalArray[0]; ++j)  
	    {
	      cout << MyGrpArray[j] << " ";            //listing of MyGrpArray    
      
	      if (ll == (GlobalArray[2]-1))
		{
		  cout << endl << "   ";
		  ll = 0;
		  ++mm;
		}
	      else
		{
		  ++ll;
		}
	      if (mm == (GlobalArray[1]))
		{
		  cout << endl << "   ";
		  mm = 0;
		}
	    } 
#endif

  return;
}

/*******************************************
/Make pixelX array
/*******************************************/
void MakeArray_pixelX (binary_type* MyGrpArray, binary_type* BinaryArray,int grp_number,int InverseDef)
{
  string loop="loop";

    if (Def[grp_number][0] == loop[0]) 
    {

#ifdef RETRIEVER_MAKE_ARRAY_PIXELX
      cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELX****************************************" << endl;
      cout << "  Def["<<grp_number<<"]= "<<Def[grp_number]<<endl;
      cout << "  GrpPara["<<grp_number<<"].init= "<<GrpPara[grp_number].init<<endl;
      cout << "  GrpPara["<<grp_number<<"].last= "<<GrpPara[grp_number].last<<endl; 
      cout << "  GrpPara["<<grp_number<<"].increment= "<<GrpPara[grp_number].increment<<endl<<endl;
#endif
      if (InverseDef==1)   //case inverse
	{

	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }

	  for (int y=0; y<GlobalArray[0];y++)
	    {
	      for (int x = GrpPara[grp_number].init; x <= GrpPara[grp_number].last; x = x + GrpPara[grp_number].increment)
		{
		  for (int tbin=0; tbin < GlobalArray[2];tbin++)
		    {
		      MyGrpArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=0;
		    }
		} 
	    }
	}
      else   //normal case
	{
	  for (int y=0; y<GlobalArray[0];y++)
	    {
	      for (int x = GrpPara[grp_number].init; x <= GrpPara[grp_number].last; x = x + GrpPara[grp_number].increment)
		{
		  for (int tbin=0; tbin < GlobalArray[2];tbin++)
		    {
		      MyGrpArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=BinaryArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])];
		    }
		} 
	    }
	}
    }  

  else
    {
      
#ifdef RETRIEVER_MAKE_ARRAY_PIXELX
      cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELX****************************************" << endl;
      cout << endl<<"List of identifiers values are: " <<endl;
      for (int i=0;i<GrpPara[grp_number].value.size();i++)
	{
	  cout << "   GrpPara["<<grp_number<<"].value["<<i<<"]= "<<GrpPara[grp_number].value[i]<<endl;
	}
      cout << endl;
#endif

      if(InverseDef==1) //case inverse
	{

	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }

	  for (int y=0; y<GlobalArray[0];y++)
	    {
	    for (int x=0; x < GrpPara[grp_number].value.size(); x++)
	      {
	      for (int tbin=0;tbin<GlobalArray[2];tbin++)
		{
		  MyGrpArray[(GrpPara[grp_number].value[x]*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=0;
		}
	      }
	    }
 	 }
      else   //normal case
	{
      for (int y=0; y<GlobalArray[0];y++)
	{
	  for (int x=0; x < GrpPara[grp_number].value.size(); x++)
	    {
	      for (int tbin=0;tbin<GlobalArray[2];tbin++)
		{
		  MyGrpArray[(GrpPara[grp_number].value[x]*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=BinaryArray[(GrpPara[grp_number].value[x]*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])];

		}
	    }
	 }
      }
    }
  return;
}

/*******************************************
/Make pixelY array
/*******************************************/
void MakeArray_pixelY (binary_type* MyGrpArray, binary_type* BinaryArray,int grp_number,int InverseDef)
{
  string loop="loop";

    if (Def[grp_number][0] == loop[0]) 
    {

#ifdef RETRIEVER_MAKE_ARRAY_PIXELY
      cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELY****************************************" << endl;
      cout << "  Def["<<grp_number<<"]= "<<Def[grp_number]<<endl;
      cout << "  GrpPara["<<grp_number<<"].init= "<<GrpPara[grp_number].init<<endl;
      cout << "  GrpPara["<<grp_number<<"].last= "<<GrpPara[grp_number].last<<endl; 
      cout << "  GrpPara["<<grp_number<<"].increment= "<<GrpPara[grp_number].increment<<endl<<endl;
#endif

      if (InverseDef==1)  //inverse case
	{

	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }

	  for (int y=GrpPara[grp_number].init; y <= GrpPara[grp_number].last; y = y + GrpPara[grp_number].increment)
	    {
	      for (int x = 0; x < GlobalArray[1]; ++x)
		{
		  for (int tbin=0; tbin < GlobalArray[2];tbin++)
		    {
		      MyGrpArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=0;
		    }
		} 
	    }
	}
      else  //normal case
	{
   for (int y=GrpPara[grp_number].init; y <= GrpPara[grp_number].last; y = y + GrpPara[grp_number].increment)
     {
       for (int x = 0; x < GlobalArray[1]; ++x)
         {
           for (int tbin=0; tbin < GlobalArray[2];tbin++)
             {
	   MyGrpArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=BinaryArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])];
	     }
	 } 
      }
     }
    }

  else
    {
      
#ifdef RETRIEVER_MAKE_ARRAY_PIXELY
      cout << endl << "****RETRIEVER_MAKE_ARRAY_PIXELY****************************************" << endl;
      cout << endl<<"List of identifiers values are: " <<endl;
      
      for (int i=0;i<GrpPara[grp_number].value.size();i++)
	{
	  cout << "   GrpPara["<<grp_number<<"].value["<<i<<"]= "<<GrpPara[grp_number].value[i]<<endl;
	}
      cout << endl;

#endif

      if (InverseDef==1)   //case inverse
	{

	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }

	 for (int y=0; y < GrpPara[grp_number].value.size(); ++y)
	{
	  for (int x=0; x < GlobalArray[1]; x++)
	    {
	      for (int tbin=0;tbin<GlobalArray[2];tbin++)
		{
		  MyGrpArray[(x*GlobalArray[2]+tbin)+(GrpPara[grp_number].value[y]*GlobalArray[2]*GlobalArray[1])]=0;
		}
	     }
	  } 
	}
      else    //normal case
	{
      for (int y=0; y < GrpPara[grp_number].value.size(); ++y)
	{
	  for (int x=0; x < GlobalArray[1]; x++)
	    {
	      for (int tbin=0;tbin<GlobalArray[2];tbin++)
		{
	         MyGrpArray[(x*GlobalArray[2]+tbin)+(GrpPara[grp_number].value[y]*GlobalArray[2]*GlobalArray[1])]= 
                   BinaryArray[(x*GlobalArray[2]+tbin)+(GrpPara[grp_number].value[y]*GlobalArray[2]*GlobalArray[1])];
		}
	    }
	}
      }
    }
  return;
}

/*******************************************
/Make Tbin array
/*******************************************/
void MakeArray_Tbin (binary_type* MyGrpArray, binary_type* BinaryArray,int grp_number,int InverseDef)
{
  string loop="loop";

    if (Def[grp_number][0] == loop[0]) 
    {

#ifdef RETRIEVER_MAKE_ARRAY_TBIN
      cout << endl << "****RETRIEVER_MAKE_ARRAY_TBIN****************************************" << endl;
      cout << "  Def["<<grp_number<<"]= "<<Def[grp_number]<<endl;
      cout << "  GrpPara["<<grp_number<<"].init= "<<GrpPara[grp_number].init<<endl;
      cout << "  GrpPara["<<grp_number<<"].last= "<<GrpPara[grp_number].last<<endl; 
      cout << "  GrpPara["<<grp_number<<"].increment= "<<GrpPara[grp_number].increment<<endl<<endl;
#endif

      if (InverseDef ==1)  //case inverse
	{
	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }

	  for (int y=0; y < GlobalArray[0]; ++y )
	    {
	      for (int x = 0; x < GlobalArray[1]; ++x)
		{
		  for (int tbin=GrpPara[grp_number].init; tbin <= GrpPara[grp_number].last; tbin = tbin + GrpPara[grp_number].increment)
		    {
		      MyGrpArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=0;
		    }
		} 
	    }
	}
      else   //normal case
	{
   for (int y=0; y < GlobalArray[0]; ++y )
     {
       for (int x = 0; x < GlobalArray[1]; ++x)
         {
           for (int tbin=GrpPara[grp_number].init; tbin <= GrpPara[grp_number].last; tbin = tbin + GrpPara[grp_number].increment)
             {
	   MyGrpArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=
	     BinaryArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])];
	     }
	 } 
      }
     }
    }
  
    else
    
    {
      
#ifdef RETRIEVER_MAKE_ARRAY_TBIN
      cout << endl << "****RETRIEVER_MAKE_ARRAY_TBIN****************************************" << endl;
      cout << endl<<"List of identifiers values are: " <<endl;
      
      for (int i=0;i<GrpPara[grp_number].value.size();i++)
	{
	  cout << "   GrpPara["<<grp_number<<"].value["<<i<<"]= "<<GrpPara[grp_number].value[i]<<endl;
	}
      cout << endl;

#endif

      if (InverseDef==1)
	{
	  for (int i=0; i<GlobalArray[0]*GlobalArray[1];i++)
	    {
	      for (int k=0; k<GlobalArray[2];k++)
		{
		  MyGrpArray[i*GlobalArray[2]+k]=BinaryArray[i*GlobalArray[2]+k];
		}
	    }

	 for (int y=0; y < GlobalArray[0]; ++y)
	{
	  for (int x=0; x < GlobalArray[1]; x++)
	    {
	      for (int tbin=0;tbin<GrpPara[grp_number].value.size();tbin++)
		{
		  MyGrpArray[(x*GlobalArray[2]+GrpPara[grp_number].value[tbin])+y*GlobalArray[2]*GlobalArray[1]]=0;
	        }
	    }
	  } 
	}
      else
	{

       for (int y=0; y < GlobalArray[0]; ++y)
	{
	  for (int x=0; x < GlobalArray[1]; x++)
	    {
	      for (int tbin=0;tbin<GrpPara[grp_number].value.size();tbin++)
		{
		  MyGrpArray[(x*GlobalArray[2]+GrpPara[grp_number].value[tbin])+y*GlobalArray[2]*GlobalArray[1]]= 
		    BinaryArray[(x*GlobalArray[2]+GrpPara[grp_number].value[tbin])+y*GlobalArray[2]*GlobalArray[1]];
	        }
	    }
	}
    }
  }

  return;
}

/*******************************************
/Make a copy of the binary Array
/*******************************************/
void  MakeArray_Everything (binary_type* MyGrpArray, binary_type* BinaryArray)
{

 for (int y=0; y<GlobalArray[0];y++)
   {
   for (int x=0; x<GlobalArray[1]; x++)
     {
     for (int tbin=0;tbin<GlobalArray[2];tbin++)
       {
	MyGrpArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=
	  BinaryArray[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])];
       }	     
     }
   }

      //  MyGrpArray = BinaryArray;
  
  return ;
}

/*******************************************
/Do the calculation between the two arrays
/*******************************************/
void DoCalculation (binary_type* GrpArray1, binary_type* GrpArray2,string Operator)
{
  string OR="OR";

  if (Operator[0] == OR[0])
    {

#ifdef RETRIEVER_MAKE_CALCULATION_OR

      cout << endl<<"****RETRIEVER_MAKE_CALCULATION************************************"<<endl;

      cout << "  --BEFORE--"<<endl<<endl;
      cout << "    GrpArray1:"<<endl;

      for (int a=0; a<GlobalArray[1];++a)
	{
	  for (int b=0; b<GlobalArray[0];++b)
	    {
	      for (int c=0; c<GlobalArray[2];++c)
		{
		  cout << GrpArray1[c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		  
		  cout << " ";
		}
	      cout << "\t";
	    }
	  cout << endl;
	}
      cout << endl;
      
   cout << "    GrpArray2:"<<endl;

   for (int a=0; a<GlobalArray[1];++a)
	{
	  for (int b=0; b<GlobalArray[0];++b)
	    {
	      for (int c=0; c<GlobalArray[2];++c)
		{
		  cout << GrpArray2[c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		  
		  cout << " ";
		}
	      cout << "\t";
	    }
	  cout << endl;
	}
      cout << endl;

#endif

#ifdef RETRIEVER_MAKE_CALCULATION_OR
      cout << "==> Operator OR"<<endl;
#endif
      int sum =0;
     for (int y=0; y<GlobalArray[0];y++)
	 {

	  for (int x=0; x<GlobalArray[1]; x++)
	     {
	       	 
	      for (int tbin=0;tbin<GlobalArray[2];tbin++)
		{	  
		
		  if ((GrpArray1[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]==0)&&
		      (GrpArray2[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]!=0))
		    {
		      GrpArray1[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=
			GrpArray2[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])];
		    }
		  
	        }
	     }
	 }

#ifdef RETRIEVER_MAKE_CALCULATION_OR
     
     cout << "  --AFTER--"<<endl<<endl;
   cout << "    GrpArray1:"<<endl;
  
  for (int a=0; a<GlobalArray[1];++a)
	{
	  for (int b=0; b<GlobalArray[0];++b)
	    {
	      for (int c=0; c<GlobalArray[2];++c)
		{
		  cout << GrpArray1[c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		  
		  cout << " ";
		}
	      cout << "\t";
	    }
	  cout << endl;
	}
      cout << endl;
   
#endif

    }
  else
    {
     
#ifdef RETRIEVER_MAKE_CALCULATION_AND

      cout << endl<<"****RETRIEVER_MAKE_CALCULATION************************************"<<endl;

      cout << "  --BEFORE--"<<endl<<endl; 
      cout << "    GrpArray1:"<<endl;

      for (int a=0; a<GlobalArray[1];++a)
	{
	  for (int b=0; b<GlobalArray[0];++b)
	    {
	      for (int c=0; c<GlobalArray[2];++c)
		{
		  cout << GrpArray1[c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		  
		  cout << " ";
		}
	      cout << "\t";
	    }
	  cout << endl;
	}
      cout << endl;
      
   cout << "    GrpArray2:"<<endl;

   for (int a=0; a<GlobalArray[1];++a)
	{
	  for (int b=0; b<GlobalArray[0];++b)
	    {
	      for (int c=0; c<GlobalArray[2];++c)
		{
		  cout << GrpArray2[c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		  
		  cout << " ";
		}
	      cout << "\t";
	    }
	  cout << endl;
	}
      cout << endl;

#endif

#ifdef RETRIEVER_MAKE_CALCULATION_AND
      cout << "==> Operator AND"<<endl;
#endif

      for (int y=0; y<GlobalArray[0];y++)
	{
	 for (int x=0; x<GlobalArray[1]; x++)
	     {
	      for (int tbin=0;tbin<GlobalArray[2];tbin++)
		{
		  if (GrpArray2[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]!=
		        GrpArray1[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])])
		    {
		      GrpArray1[(x*GlobalArray[2]+tbin)+(y*GlobalArray[2]*GlobalArray[1])]=0;
		    }	     
		}
	     }
	 }

#ifdef RETRIEVER_MAKE_CALCULATION_AND

      cout << "  --AFTER--"<<endl<<endl;
      cout << "    GrpArray1:"<<endl;
  
  for (int a=0; a<GlobalArray[1];++a)
	{
	  for (int b=0; b<GlobalArray[0];++b)
	    {
	      for (int c=0; c<GlobalArray[2];++c)
		{
		  cout << GrpArray1[c+b*GlobalArray[1]*GlobalArray[2]+a*GlobalArray[2]];
		  
		  cout << " ";
		}
	      cout << "\t";
	    }
	  cout << endl;
	}
      cout << endl;
   
#endif

    }
  return;
}

#ifdef SWAP_ENDIAN
/*******************************************
/To swap from little endian to big endian
/*******************************************/
inline void endian_swap (binary_type& x)
{
  x = (x>>24) |
    ((x<<8) & 0x00FF0000) |
    ((x>>8) & 0x0000FF00) |
    (x<<24);
}
#endif
