#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>

#include "ConvexHull.h"

#define DMaxArboles 	25
#define DMaximoCoste 999999


  //////////////////////////
 // Estructuras de datos //
//////////////////////////


// Tree structure definition.
struct  Arbol
{
	int	  IdArbol;
	Point Coord;			// tree position
	int Valor;				// Value
	int Longitud;			// wood quantity
};
typedef struct Arbol TArbol, *PtrArbol;



// Forest structure definition.
struct Bosque
{
	int 		NumArboles;
	TArbol 	Arboles[DMaxArboles];
};
typedef struct Bosque TBosque, *PtrBosque; 


typedef struct 
{
	int Id;
	int Coste;
} EstructuraCombinacion;


// Problem respresentation
struct ListaArboles
{
	int 		NumArboles;
 	float		Coste;
	float		CosteArbolesCortados;
	float		CosteArbolesRestantes;
	float		LongitudCerca;
	float		MaderaSobrante;
	int 		Arboles[DMaxArboles];
};
typedef struct ListaArboles TListaArboles, *PtrListaArboles;



// static coordinates.
typedef Point TVectorCoordenadas[DMaxArboles], *PtrVectorCoordenadas;



typedef enum {false, true} bool;


  ////////////////////////
 // Variables Globales //
////////////////////////

TBosque ArbolesEntrada;



  //////////////////////////
 // Funtion definition   //
//////////////////////////
bool LeerFicheroEntrada(char *PathFicIn);
bool GenerarFicheroSalida(TListaArboles optimo, char *PathFicOut);
bool CalcularCercaOptima(PtrListaArboles Optimo);
void OrdenarArboles();
bool CalcularCombinacionOptima(int PrimeraCombinacion, int UltimaCombinacion, PtrListaArboles Optimo);
int EvaluarCombinacionListaArboles(int Combinacion);
int ConvertirCombinacionToArboles(int Combinacion, PtrListaArboles CombinacionArboles);
int ConvertirCombinacionToArbolesTalados(int Combinacion, PtrListaArboles CombinacionArbolesTalados);
void ObtenerListaCoordenadasArboles(TListaArboles CombinacionArboles, TVectorCoordenadas Coordenadas);
float CalcularLongitudCerca(TVectorCoordenadas CoordenadasCerca, int SizeCerca);
float CalcularDistancia(int x1, int y1, int x2, int y2);
int CalcularMaderaArbolesTalados(TListaArboles CombinacionArboles);
int CalcularCosteCombinacion(TListaArboles CombinacionArboles);
void MostrarArboles(TListaArboles CombinacionArboles);
void MiReduccion(void *in, void *inout, int *len, MPI_Datatype *dptr);

int world_rank;
int world_size;

int main(int argc, char *argv[])
{
	MPI_Init(NULL, NULL);

	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	TListaArboles Optimo;
	char *posicion;
	int namesrclen;
	unsigned long len;
	char dest[255],outfile[255];
	double tpivot1=0,tpivot2=0; //time counting
	struct timeval tim;

	if (argc<2 || argc>3)
		printf("Error Argumentos");

	//Capture first token time
    gettimeofday(&tim, NULL);
    tpivot1 = tim.tv_sec+(tim.tv_usec/1000000.0);

	if (!LeerFicheroEntrada(argv[1]))
	{
		printf("Error opening input file\n");
		exit(1);
	}

	if (!CalcularCercaOptima(&Optimo))
	{
		printf("Error computing the optimal solution\n");
		exit(1);
	}

	if(world_rank == 0)
	{
	 if (argc==2)
		{
			len=strlen(argv[1]);
			posicion = strchr(argv[1], '.');
			memset(dest, '\0', sizeof(dest));
			namesrclen=posicion-argv[1];
			strncpy(dest,argv[1],namesrclen);
			sprintf(outfile,"%s.res", dest);

			if (!GenerarFicheroSalida(Optimo, outfile))
			{
				printf("Error writing output file\n");
				exit(1);
			}
		}
		else
		{
			if (!GenerarFicheroSalida(Optimo, argv[2]))
			{
				printf("Error writing output file\n");
				exit(1);
			}
		}

		gettimeofday(&tim, NULL);
		tpivot2 = (tim.tv_sec+(tim.tv_usec/1000000.0));
		printf("\n+%.6lf\n", tpivot2-tpivot1);
	}
	
	exit(0);
	MPI_Finalize();
}


bool LeerFicheroEntrada(char *PathFicIn)
{
	FILE *FicIn;
	int a;

	FicIn=fopen(PathFicIn,"r");
	if (FicIn==NULL)
	{
		perror("Opening input file");
		return false;
	}
	printf("Input data:\n");

	// Reading the number of trees in the input woods.
	if (fscanf(FicIn, "%d", &(ArbolesEntrada.NumArboles))<1)
	{
		perror("Reading input woods");
		return false;
	}
	printf("\tTrees: %d.\n",ArbolesEntrada.NumArboles);

	// Reading tree atributes.
	for(a=0;a<ArbolesEntrada.NumArboles;a++)
	{
		ArbolesEntrada.Arboles[a].IdArbol=a+1;
		// Reading  x, y, value, Longitud.
		if (fscanf(FicIn, "%d %d %d %d",&(ArbolesEntrada.Arboles[a].Coord.x), &(ArbolesEntrada.Arboles[a].Coord.y), &(ArbolesEntrada.Arboles[a].Valor), &(ArbolesEntrada.Arboles[a].Longitud))<4)
		{
			perror("Reading tree info");
			return false;
		}
		printf("\tTree %d-> (%d,%d) Value:%d, Long:%d.\n",a+1,ArbolesEntrada.Arboles[a].Coord.x, ArbolesEntrada.Arboles[a].Coord.y, ArbolesEntrada.Arboles[a].Valor, ArbolesEntrada.Arboles[a].Longitud);
	}

	return true;
}



bool GenerarFicheroSalida(TListaArboles Optimo, char *PathFicOut)
{
	FILE *FicOut;
	int a;

	FicOut=fopen(PathFicOut,"w+");
	if (FicOut==NULL)
	{
		perror("Opening output file.");
		return false;
	}

	// Writting the number of trees to cut.
	if (fprintf(FicOut, "Cutting %3d trees           : ", Optimo.NumArboles)<1)
	{
		perror("writing number of cutting trees");
		return false;
	}

	for(a=0;a<Optimo.NumArboles;a++)
	{
		// Writing the id of a tree to cut.
		if (fprintf(FicOut, "%3d ",ArbolesEntrada.Arboles[Optimo.Arboles[a]].IdArbol)<1)
		{
			perror("writing id of a tree to cut.");
			return false;
		}
	}

	// Writing the total missing wood
	if (fprintf(FicOut, "\nFence long./missing Wood    : \t%6.2f (%6.2f)",   Optimo.LongitudCerca, Optimo.MaderaSobrante)<1)
	{
		perror("writing total missing wood");
		return false;
	}

	// Writing the cost of trees to cut
	if (fprintf(FicOut, "\nValue of the cutted Trees   : \t%6.2f", Optimo.CosteArbolesCortados)<1)
	{
		perror("writing value of the cutted Trees");
		return false;
	}

	// Writing the value of the remaining trees
	if (fprintf(FicOut, "\nValue of the Remaining Trees: \t%6.2f\n", Optimo.CosteArbolesRestantes)<1)
	{
		perror("writing value of the Remaining Trees");
		return false;
	}

	return true;
}



bool CalcularCercaOptima(PtrListaArboles Optimo)
{
	int MaxCombinaciones;

	/* Computing total combinations */
	MaxCombinaciones = (int) pow(2.0,ArbolesEntrada.NumArboles);

	// Sort trees in increasing order by x,y
	OrdenarArboles();

	/* Computing optimal */
	Optimo->NumArboles = 0;
	Optimo->Coste = DMaximoCoste;
	CalcularCombinacionOptima(1, MaxCombinaciones, Optimo);

	return true;
}



void OrdenarArboles()
{
  int a,b = 0;

	for(a=0; a<(ArbolesEntrada.NumArboles-1); a++)
	{
		for(b=a; b<ArbolesEntrada.NumArboles; b++)
		{
			if ( ArbolesEntrada.Arboles[b].Coord.x < ArbolesEntrada.Arboles[a].Coord.x ||
				 (ArbolesEntrada.Arboles[b].Coord.x == ArbolesEntrada.Arboles[a].Coord.x && ArbolesEntrada.Arboles[b].Coord.y < ArbolesEntrada.Arboles[a].Coord.y) )
			{
				TArbol aux;

				// aux=a
				aux.Coord.x = ArbolesEntrada.Arboles[a].Coord.x;
				aux.Coord.y = ArbolesEntrada.Arboles[a].Coord.y;
				aux.IdArbol = ArbolesEntrada.Arboles[a].IdArbol;
				aux.Valor = ArbolesEntrada.Arboles[a].Valor;
				aux.Longitud = ArbolesEntrada.Arboles[a].Longitud;

				// a=b
				ArbolesEntrada.Arboles[a].Coord.x = ArbolesEntrada.Arboles[b].Coord.x;
				ArbolesEntrada.Arboles[a].Coord.y = ArbolesEntrada.Arboles[b].Coord.y;
				ArbolesEntrada.Arboles[a].IdArbol = ArbolesEntrada.Arboles[b].IdArbol;
				ArbolesEntrada.Arboles[a].Valor = ArbolesEntrada.Arboles[b].Valor;
				ArbolesEntrada.Arboles[a].Longitud = ArbolesEntrada.Arboles[b].Longitud;

				// b=aux
				ArbolesEntrada.Arboles[b].Coord.x = aux.Coord.x;
				ArbolesEntrada.Arboles[b].Coord.y = aux.Coord.y;
				ArbolesEntrada.Arboles[b].IdArbol = aux.IdArbol;
				ArbolesEntrada.Arboles[b].Valor = aux.Valor;
				ArbolesEntrada.Arboles[b].Longitud = aux.Longitud;
			}
		}
	}
}

void MiReduccion(void *in, void *inout, int *len, MPI_Datatype *dptr)
{ 
    EstructuraCombinacion mejor_combinacion;
	mejor_combinacion.Coste = 999999;
	int i;
	
	EstructuraCombinacion * estructura_combinacion_input = (EstructuraCombinacion *) in;
	EstructuraCombinacion * estructura_combinacion_output = (EstructuraCombinacion *) inout;

	for (i=0; i< *len; i++) { 
        if(estructura_combinacion_input->Coste < mejor_combinacion.Coste)
		{
			mejor_combinacion.Coste = estructura_combinacion_input->Coste;
			mejor_combinacion.Id = estructura_combinacion_input->Id;
		}
		in++;
    }

	estructura_combinacion_output->Id = mejor_combinacion.Id;
	estructura_combinacion_output->Coste = mejor_combinacion.Coste;

	inout = (void *)estructura_combinacion_output;
} 


// Computing the optimal combination in a range
bool CalcularCombinacionOptima(int PrimeraCombinacion, int UltimaCombinacion, PtrListaArboles Optimo)
{
	int Coste, Combinacion, MejorCombinacion=0, CosteMejorCombinacion;

	TListaArboles CombinacionArboles;
	TVectorCoordenadas CoordArboles, CercaArboles;
	int NumArboles, PuntosCerca;
	float MaderaArbolesTalados;

  	printf("Evaluating combinations: \n");
	CosteMejorCombinacion = Optimo->Coste;

	MPI_Op myOp; 
    MPI_Datatype ctype; 
    MPI_Type_contiguous(2, MPI_INT, &ctype ); 
    MPI_Type_commit( &ctype ); 
    MPI_Op_create( MiReduccion, true, &myOp );

	EstructuraCombinacion MejorCombinacion_Parcial, MejorCombinacion_Global;
	
	#pragma omp parallel for schedule(static) private(Coste)
	for (Combinacion=PrimeraCombinacion; Combinacion<UltimaCombinacion; Combinacion = Combinacion + world_size)
	{
		printf("\tC%d -> \t",Combinacion);
		Coste = EvaluarCombinacionListaArboles(Combinacion + world_rank);

		if ( Coste < CosteMejorCombinacion )
		#pragma omp critical
		if ( Coste < CosteMejorCombinacion )
		{
			CosteMejorCombinacion = Coste;
			MejorCombinacion = Combinacion + world_rank;
			printf("***");
		}
		printf("\n");
	}

	MejorCombinacion_Parcial.Id = MejorCombinacion;
	MejorCombinacion_Parcial.Coste = CosteMejorCombinacion;

	MPI_Reduce(&MejorCombinacion_Parcial, &MejorCombinacion_Global, 1, ctype, myOp, 0, MPI_COMM_WORLD);
	
	if (world_rank == 0) {
		if (MejorCombinacion_Global.Coste == Optimo->Coste)
			return false;  // No se ha encontrado una combinacin mejor.

		// Asignar combinacin encontrada.
		ConvertirCombinacionToArbolesTalados(MejorCombinacion, Optimo);
		Optimo->Coste = MejorCombinacion_Global.Coste;
		// Calcular estadisticas óptimo.
		NumArboles = ConvertirCombinacionToArboles(MejorCombinacion, &CombinacionArboles);
		ObtenerListaCoordenadasArboles(CombinacionArboles, CoordArboles);
		PuntosCerca = chainHull_2D( CoordArboles, NumArboles, CercaArboles );

		Optimo->LongitudCerca = CalcularLongitudCerca(CercaArboles, PuntosCerca);
		MaderaArbolesTalados = CalcularMaderaArbolesTalados(*Optimo);
		Optimo->MaderaSobrante = MaderaArbolesTalados - Optimo->LongitudCerca;
		Optimo->CosteArbolesCortados = MejorCombinacion_Global.Coste;
		Optimo->CosteArbolesRestantes = CalcularCosteCombinacion(CombinacionArboles);

		return true;
  	}

	else
	{
		MPI_Finalize();
	}
}


int EvaluarCombinacionListaArboles(int Combinacion)
{
	TVectorCoordenadas CoordArboles, CercaArboles;
	TListaArboles CombinacionArboles, CombinacionArbolesTalados;
	int NumArboles, NumArbolesTalados, PuntosCerca, CosteCombinacion;
	float LongitudCerca, MaderaArbolesTalados;

	// Convertimos la combinacin al vector de arboles no talados.
	NumArboles = ConvertirCombinacionToArboles(Combinacion, &CombinacionArboles);

	// Obtener el vector de coordenadas de arboles no talados.
	ObtenerListaCoordenadasArboles(CombinacionArboles, CoordArboles);

	// Calcular la cerca
	PuntosCerca = chainHull_2D( CoordArboles, NumArboles, CercaArboles );

	/* Evaluar si obtenemos suficientes �boles para construir la cerca */
	LongitudCerca = CalcularLongitudCerca(CercaArboles, PuntosCerca);

	// Evaluar la madera obtenida mediante los arboles talados.
	// Convertimos la combinacin al vector de arboles no talados.
	NumArbolesTalados = ConvertirCombinacionToArbolesTalados(Combinacion, &CombinacionArbolesTalados);
	printf(" %d arboles cortados: ",NumArbolesTalados);
	MostrarArboles(CombinacionArbolesTalados);
	MaderaArbolesTalados = CalcularMaderaArbolesTalados(CombinacionArbolesTalados);
	printf("  Madera:%4.2f  \tCerca:%4.2f ",MaderaArbolesTalados, LongitudCerca);
	if (LongitudCerca > MaderaArbolesTalados)
		{
			// Los arboles cortados no tienen suficiente madera para construir la cerca.
		printf("\tCoste:%d",DMaximoCoste);
		return DMaximoCoste;
		}

	// Evaluar el coste de los arboles talados.
	CosteCombinacion = CalcularCosteCombinacion(CombinacionArbolesTalados);
	printf("\tCoste:%d",CosteCombinacion);

	return CosteCombinacion;
}


int ConvertirCombinacionToArboles(int Combinacion, PtrListaArboles CombinacionArboles)
{
	int arbol=0;

	CombinacionArboles->NumArboles=0;
	CombinacionArboles->Coste=0;

	while (arbol<ArbolesEntrada.NumArboles)
	{
		if ((Combinacion%2)==0)
		{
			CombinacionArboles->Arboles[CombinacionArboles->NumArboles]=arbol;
			CombinacionArboles->NumArboles++;
			CombinacionArboles->Coste+= ArbolesEntrada.Arboles[arbol].Valor;
		}
		arbol++;
		Combinacion = Combinacion>>1;
	}

	return CombinacionArboles->NumArboles;
}


int ConvertirCombinacionToArbolesTalados(int Combinacion, PtrListaArboles CombinacionArbolesTalados)
{
	int arbol=0;

	CombinacionArbolesTalados->NumArboles=0;
	CombinacionArbolesTalados->Coste=0;

	while (arbol<ArbolesEntrada.NumArboles)
	{
		if ((Combinacion%2)==1)
		{
			CombinacionArbolesTalados->Arboles[CombinacionArbolesTalados->NumArboles]=arbol;
			CombinacionArbolesTalados->NumArboles++;
			CombinacionArbolesTalados->Coste+= ArbolesEntrada.Arboles[arbol].Valor;
		}
		arbol++;
		Combinacion = Combinacion>>1;
	}

	return CombinacionArbolesTalados->NumArboles;
}



void ObtenerListaCoordenadasArboles(TListaArboles CombinacionArboles, TVectorCoordenadas Coordenadas)
{
	int c, arbol;

	for (c=0;c<CombinacionArboles.NumArboles;c++)
	{
    arbol=CombinacionArboles.Arboles[c];
		Coordenadas[c].x = ArbolesEntrada.Arboles[arbol].Coord.x;
		Coordenadas[c].y = ArbolesEntrada.Arboles[arbol].Coord.y;
	}
}



float CalcularLongitudCerca(TVectorCoordenadas CoordenadasCerca, int SizeCerca)
{
	int x;
	float coste;

	for (x=0;x<(SizeCerca-1);x++)
	{
		coste+= CalcularDistancia(CoordenadasCerca[x].x, CoordenadasCerca[x].y, CoordenadasCerca[x+1].x, CoordenadasCerca[x+1].y);
	}

	return coste;
}



float CalcularDistancia(int x1, int y1, int x2, int y2)
{
	return(sqrt(pow((double)abs(x2-x1),2.0)+pow((double)abs(y2-y1),2.0)));
}



int CalcularMaderaArbolesTalados(TListaArboles CombinacionArboles)
{
	int a;
	int LongitudTotal=0;

	for (a=0;a<CombinacionArboles.NumArboles;a++)
	{
		LongitudTotal += ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].Longitud;
	}

	return(LongitudTotal);
}



int CalcularCosteCombinacion(TListaArboles CombinacionArboles)
{
	int a;
	int CosteTotal=0;

	for (a=0;a<CombinacionArboles.NumArboles;a++)
	{
		CosteTotal += ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].Valor;
	}

	return(CosteTotal);
}



void MostrarArboles(TListaArboles CombinacionArboles)
{
	int a;

	for (a=0;a<CombinacionArboles.NumArboles;a++)
		printf("%d ",ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].IdArbol);

  	for (;a<ArbolesEntrada.NumArboles;a++)
    	printf("  ");
}
