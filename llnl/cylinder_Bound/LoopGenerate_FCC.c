#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "Home.h"
#include "WriteProp.h"
#include "Util.h"
#include "Comm.h"
#include "Mobility.h"
#include <math.h>
#ifdef _CYLINDER
#include "CYL.h"
#endif
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
 
#define DEBUG_kMC 0
#define DEBUG_GB 0

const gsl_rng *gBaseRand;       /* global rand number generator */

#define DEBUG_PRINT 0
#define DEBUG_PRINTGB 0
#define DEBUG_PRINT1 0
#define PI 3.14159265

/*---------------------------------------------------------------------------
 *
 *	Function:	Make_NucSites
 *	Description:	This function generate following arrays
 *			 for possible nucleation sites (2014.11.07)
 *
 *	ARRAYs:		Make a dislocation loop at random position on the surface. 
 *		NucSite_x[NumNucSiteAllD] - x coordinate 
 *		NucSite_y[NumNucSiteAllD] - y coordinate      
 *		NucSite_z[NumNucSiteAllD] - z coordinate
 *		NucSite_SCF[NumNucSiteAllD]-Stress concentation factor		
 *
 *-------------------------------------------------------------------------*/

void Make_NucSites(Home_t *home, Cylinder_t *cylinder)
{
    int l,k;
    real8 randnum1, randnum2, randnum3, height;
    Param_t *param;
    param = home->param;
    real8 radius =param->cyl_radius; 
    real8 StressCon;

    FILE  *fp;
    char  fileName[256];

     /* # of nulceation site is set proportional to the diameter 
 * 	e.g) D=150nm  NumNucSiteD = 150 
 * 	e.g) D=1000nm NumNucSiteD = 1000 */
    int NumNucSiteD = (int) (param->cyl_radius*1.0*param->burgMag*1e9+1.0);

#ifdef _BOUNDARY
     /* To allow nulceations on GB, the total number of nucleation site is increase by 1/PI */
    int NumNucSiteBoundD = (int) (NumNucSiteD/PI);
    int NumNucSiteAllD = NumNucSiteD + NumNucSiteBoundD;
    real8   x0, y0, z0;
    real8   x1, y1, z1;
    real8   x2, y2, z2;
    real8	tol = 1e-3;  	 

	//  Internal Boundary  : Tx*x + Ty*y + Tz*z + Td = 0 
	real8 Tx,Ty,Tz,Td,normT;
	Tx = param->IntBoundary[0];
	Ty = param->IntBoundary[1];
	Tz = param->IntBoundary[2];
	Td = param->IntBoundary[3];
	normT = sqrt(Tx*Tx+Ty*Ty+Tz*Tz); 
	Tx/=normT;	Ty/=normT;	Tz/=normT;	Td/=normT; //normalize it 

	real8   LoopR = param->LoopRadius;
/*
    if (DEBUG_GB){
        printf("*** GB nucleation sites *** \n");
        printf("NumNucSiteD = %d   NumNucSiteBoundD = %d  NumNucSiteAllD = %d \n",NumNucSiteD, NumNucSiteBoundD, NumNucSiteAllD);
    }
*/
#else
    int NumNucSiteAllD = NumNucSiteD;
#endif

    if (NumNucSiteAllD > NumNucSite){
        Fatal("NumNucSite(=%d) in CYL.h should be larger than NumNucSiteAllD(=%d)",NumNucSite, NumNucSiteAllD);
    }

    /* Generate nulceation sites on free surface */

    for (l = 0; l < NumNucSiteD ; l++){
        randnum1 =rand()/(double)RAND_MAX; 
        randnum2 =rand()/(double)RAND_MAX;
        height = param->zBoundMax-param->zBoundMin;
        
        // Loop center 
        cylinder->NucSite_x[l] = radius*cos(2.0*PI*randnum1);  
        cylinder->NucSite_y[l] = radius*sin(2.0*PI*randnum1);  
        cylinder->NucSite_z[l] = (randnum2 - 0.5)*height;      
        
        do{
            /* Stress concentration factor is set to larger than zero and less than 3.0 */
            StressCon = randn(param->NucSTRESSCon,param->NucSTRESSConSD);
            StressCon = fabs(StressCon);

            cylinder->NucSite_SCF[l] = StressCon;
        }
        while ( (StressCon <= 0.0) && (StressCon >=3.0));
    }

#ifdef _BOUNDARY
    /* Generate nulceation sites on GB */
    for (l = NumNucSiteD; l < NumNucSiteAllD ; l++){
        randnum1 =rand()/(double)RAND_MAX; 
        randnum2 =rand()/(double)RAND_MAX;
        randnum3 =rand()/(double)RAND_MAX;
        height = param->zBoundMax-param->zBoundMin; 
        
        // Loop center
        x0 = radius*cos(2.0*PI*randnum1);  
        y0 = radius*sin(2.0*PI*randnum1);  
        z0 = (randnum2 - 0.5)*height;      

        // Projected to the GB plane
        x1 = x0 - (Tx*(Td + Tx*x0 + Ty*y0 + Tz*z0))/(Tx*Tx + Ty*Ty + Tz*Tz);
        y1 = y0 - (Ty*(Td + Tx*x0 + Ty*y0 + Tz*z0))/(Tx*Tx + Ty*Ty + Tz*Tz);
        z1 = z0 - (Tz*(Td + Tx*x0 + Ty*y0 + Tz*z0))/(Tx*Tx + Ty*Ty + Tz*Tz);


/* To avoid complexity, the center of the dislocation loop offset from the center. 

        // Check the nucleation center is located on GB plane
        if (fabs(Tx*x1 + Ty*y1 + Tz*z1 + Td)>tol){
            Fatal("GB nulceation site (%f,%f,%f) is located out of the GB plane!)",x1,y1,z1);
        }
*/
        if (randnum3>0.5){
            x2 = x1 + LoopR*Tx;
            y2 = y1 + LoopR*Ty;
            z2 = z1 + LoopR*Tz;
        }
        else{
            x2 = x1 - LoopR*Tx;
            y2 = y1 - LoopR*Ty;
            z2 = z1 - LoopR*Tz;
        }

        // Assign the GB nucleation sites
        cylinder->NucSite_x[l] = x2;
        cylinder->NucSite_y[l] = y2;
        cylinder->NucSite_z[l] = z2;

        do{ // Stress concentration factor is set to larger than zero and less than 5.0
            StressCon = randn(param->NucSTRESSConGB,param->NucSTRESSConSD);
            StressCon = fabs(StressCon);
            
            cylinder->NucSite_SCF[l] = StressCon;
        }
        while ( (StressCon <= 0.0) && (StressCon >=5.0));
    }
#endif
/* 
    // Save the data for nucleation sites in a file(kMC_NucSite)
    if (DEBUG_kMC){
        snprintf(fileName, sizeof(fileName), "%s/kMC_NucSite",DIR_PROPERTIES);
        fp = fopen(fileName, "a");
        for (k = 0; k < NumNucSiteAllD ; k++){
            fprintf(fp,"%e\t%e\t%e\t%e\t%d\n",cylinder->NucSite_x[k],
            cylinder->NucSite_y[k],cylinder->NucSite_z[k],
            cylinder->NucSite_SCF[k],cylinder->NucSite_Num[k]);
        }
        
        fclose(fp);
    }
*/
    return;
}

/*---------------------------------------------------------------------------
 *
 *	Function:	Compute_Nuc_Probability
 *	Description:	This function compute nucleation probability 
 *			 for each site (2014.11.07)
 *
 *	ARRAYs:		Make a dislocation loop at random position on the surface. 
 *			NucSite_P[NumNucSiteD] - Individual probability
 *			NucSite_R[NumNucSiteD] - Cumulative probability
 *
 * 	Ref : 		Entropic effect on the rate of dislocation nucleation
 *      	 	PNAS(2011) by Seunghwa Ryu et al.
 *-------------------------------------------------------------------------*/

void Compute_Nuc_Probability(Home_t *home, Cylinder_t *cylinder)
{

    int    l,k;
    Param_t *param;
    param = home->param;
    real8  TEMP = param->NucTEMP;
    real8  KbT=8.6173324*1e-5*TEMP;		// [eV]
    real8  Schmid = 0.408248290463863;  // Schmid factor in FCC
    real8  Accumulated_Prob;            // Acculumated nucleation probability 
    /*  Caution:
     *  Currently, we use a constant Schmid factor, 
     * since the loading direction is fixed to [100]. 
     * It needs to be changed in terms of loading axis,
     * when we could specify the loading direction in ctrl file.
     */

     /* # of nulceation site is set proportional to the diameter 
 * 	e.g) D=150nm  NumNucSiteD = 150 
 * 	e.g) D=1000nm NumNucSiteD = 1000 */
    int NumNucSiteD = (int) (param->cyl_radius*1.0*param->burgMag*1e9+1.0);

#ifdef _BOUNDARY
     /* To allow nulceations on GB, the total number of nucleation site is increase by 1/PI */ 
    int NumNucSiteBoundD = (int) (NumNucSiteD/PI);
    int NumNucSiteAllD = NumNucSiteD + NumNucSiteBoundD;
/*
    if (DEBUG_GB){
        printf("*** GB nucleation sites in Compute_Nuc_Probability*** \n");
        printf("NumNucSiteD = %d   NumNucSiteBoundD = %d  NumNucSiteAllD = %d \n"
                ,NumNucSiteD, NumNucSiteBoundD, NumNucSiteAllD);
    }
*/
#else
    int NumNucSiteAllD = NumNucSiteD;
#endif

    real8  SS1, MinSS1, Q[NumNucSiteAllD], dtt;
    real8  aa =  4.811799e+00;
    real8  bb = -2.359345e+00;
    real8  cc =  4.742173e-03;
    real8  dd = -2.457447e+00;
    real8  ee = -1.330434e-01;
    real8  v0 = 1e13;				// Nucleation frequency ~ Debye frequency

    real8  mu = param->shearModulus;
    real8  radius =param->cyl_radius; 
    real8  al, am, an, amag, sigijk; 

    real8  dS,ep_slip,thetap_slip,thetaElastic;
    real8  S = 1.0 ;
    real8  Young  = 2*mu*(1.0 + param->pois);	// Young's modulus
    real8  beta   = 22.6375*M_PI/180;             	// Angle between slip plane and loading direction[rad]
    real8  L0     = 10.0*radius;
    int    Slip;					// Number of slip per sites	
    real8  SCF_Old, SCF_New;
    real8	SS[3][3];

#if defined _CYLINDER && _TORSION /*iryu*/
    real8	ThetaEla;
#endif

    FILE  *fp;
    char  fileName[256];

    dtt = param->realdt;

    for (k = 0; k < NumNucSiteAllD ; k++){

        Slip = cylinder->NucSite_Num[k];		// # of nucleation at kth Site

/* 
 * Resolve shear stress is used as the NucSTRESS
 */
        al = param->edotdir[0];
     	am = param->edotdir[1];
     	an = param->edotdir[2];

    	amag = sqrt(al*al+am*am+an*an);

    	al /= amag;
    	am /= amag;
    	an /= amag;

        SS[0][0] = param->appliedStress[0];
		SS[1][1] = param->appliedStress[1];
		SS[2][2] = param->appliedStress[2];
		SS[1][2] = param->appliedStress[3];
		SS[0][2] = param->appliedStress[4];
		SS[0][1] = param->appliedStress[5];

#if defined _CYLINDER && _TORSION /*iryu*/
		ThetaEla = param->AppliedTheta*M_PI/180;
		/* Pure torsion + applied stress in Z direction */
		SS[1][2] = SS[1][2] + radius*mu*ThetaEla;
		SS[0][2] = SS[0][2] - radius*mu*ThetaEla;
#endif
		SS[2][0] = SS[0][2];
		SS[2][1] = SS[1][2];
		SS[1][0] = SS[0][1];

    	sigijk = SS[0][0]*al*al     +
	             SS[1][1]*am*am     +
	             SS[2][2]*an*an     +
	             2.0*SS[1][2]*am*an +
    	         2.0*SS[0][2]*an*al +
	             2.0*SS[0][1]*al*am;	// Stress
    	sigijk = fabs(sigijk);
	    param->NucSTRESS = sigijk*Schmid;

/* 
 * Adjust SCF for nucleation site to delocalize plastic deformation from surface nucleation 
 */
#if defined _CYLINDER && _TORSION /*iryu*/
#if defined _TENSIONafterTORSION
/* 
 * Delocalize nucleation as same as tension
 */
		ep_slip = Slip/L0*cos(beta); 
		SCF_Old = cylinder->NucSite_SCF[k];
		dS = Slip*sqrt(4*radius*radius - Slip*Slip)/2 + 2*radius*radius*asin(Slip/(2.0*radius));// Area change 
		if (param->NucLocal == 1){
			SCF_New = SCF_Old;
		}
		else{
			SCF_New = SCF_Old*(1.0-(param->NucLocalCoeff)*Young*ep_slip/sigijk)*(M_PI*radius*radius/(M_PI*radius*radius-dS));
		}
#else
		param->NucSTRESS = mu*radius*(param->AppliedTheta*M_PI/180)*Schmid;
		thetaElastic = param->AppliedTheta;	
		thetap_slip = Slip/(M_PI*radius*radius)*(180/M_PI);
		SCF_Old = cylinder->NucSite_SCF[k];
			
		if (param->NucLocal == 1){
            SCF_New = SCF_Old;
        }
        else{
            SCF_New = SCF_Old*(1.0-(param->NucLocalCoeff)*thetap_slip/thetaElastic)*(L0/(L0-Slip));
        }
#endif
#else
		ep_slip = Slip/L0*cos(beta); 
		SCF_Old = cylinder->NucSite_SCF[k];
		dS = Slip*sqrt(4*radius*radius - Slip*Slip)/2 + 2*radius*radius*asin(Slip/(2.0*radius));// Area change 
	
		if (param->NucLocal == 1){
    		SCF_New = SCF_Old;
    	}
    	else{
    		SCF_New = SCF_Old*(1.0-(param->NucLocalCoeff)*Young*ep_slip/sigijk)*(M_PI*radius*radius/(M_PI*radius*radius-dS));
    	}
#endif
/* 
 * Note that NucSTRESS is the value of resolved shear stress
 * SS1 is the corresponding stress in [001] direction
 */
	    SS1 = param->NucSTRESS*SCF_New*(1.0/Schmid);	
        MinSS1 = 1.e9;
/* To avoid numerical error, set the minimum stress. 
 * For SS=MinSS1, Probability is still pretty small
 */
    	if (SS1 <= MinSS1) SS1 = MinSS1; 
	     	
    	Q[k]  = aa*pow((SS1/1e9),bb)-cc*TEMP*pow((SS1/1e9),dd) + ee;

    	cylinder->NucSite_P[k] = dtt*v0*exp(-Q[k]/KbT);

    	cylinder->NucSite_R[k] = 0.0;
    	for (l = 0; l < k+1 ; l++){
            cylinder->NucSite_R[k] = cylinder->NucSite_R[k] + cylinder->NucSite_P[l];
        } 

    }

    Accumulated_Prob = cylinder->NucSite_R[NumNucSiteAllD-1];
    // printf("k =%d , Accumulated nucleation probability = %f\n",NumNucSiteAllD-1, Accumulated_Prob);


/* If accumulated probability is larger than 1, all probability need te be adjusted 
 * to make sure sum of all probability is less or equal to 1
 */
    if (Accumulated_Prob > 1.0){
        printf("Accumulated nucleation probability = %f is scaled,"
               "to make sure that all sum of probability is unity.\n",Accumulated_Prob);
        for (k = 0; k < NumNucSiteAllD ; k++){
            cylinder->NucSite_R[k]/=Accumulated_Prob;
        }
    }

    return;
}

/*---------------------------------------------------------------------------
 *
 *	Function:	Find_Nucleation_Sites
 *	Description:	This function is to find the nucleation site 
 *			with respect to the nucleation rate (2014.11.10)
 *
 *	Algorithm:(Kinetic Monte Carlo)
 *          1. Generate random number, u
 *			2. Find the nucleation site i such that 
 *				R(i-1) <= uR(N) <= R(i)
 *
 *	ARRAYs:		NucSite_F[NumNucSiteD] - Nucleation Flag - 1: Nucleation 
 *
 *-------------------------------------------------------------------------*/

void Find_Nucleation_Sites(Home_t *home, Cylinder_t *cylinder)
{
     int    i,k;
     real8  randnum, u, R1, R2;
     int    randtest;
     Param_t *param;
     param = home->param;
     real8 dtt = param->realdt;

     real8        randVal;
     static int   seed = 8917346;

     /* # of nulceation site is set proportional to the diameter 
 * 	e.g) D=150nm  NumNucSiteD = 150 
 * 	e.g) D=1000nm NumNucSiteD = 1000 */
     int NumNucSiteD = (int) (param->cyl_radius*1.0*param->burgMag*1e9+1.0);

#ifdef _BOUNDARY
     /* To allow nulceations on GB, the total number of nucleation site is increase by 1/PI */ 
    int NumNucSiteBoundD = (int) (NumNucSiteD/PI);
    int NumNucSiteAllD = NumNucSiteD + NumNucSiteBoundD;
/*
    if (DEBUG_GB){
        printf("*** GB nucleation sites in Find_Nucleation_Sites *** \n");
        printf("NumNucSiteD = %d   NumNucSiteBoundD = %d  NumNucSiteAllD = %d \n"
                ,NumNucSiteD, NumNucSiteBoundD, NumNucSiteAllD);
    }
*/
#endif

     FILE  *fp;
     char  fileName[256];

     randVal = randm(&seed);

     for (i = 0; i < NumNucSiteAllD ; i++)	cylinder->NucSite_F[i] = 0;

/*--------------- Only for the nucleatoin test ------------
     if (home->cycle < 20){
	     cylinder->NucSite_F[home->cycle] = 1; 
     }
----------------- Only for the nucleatoin test -----------*/

// Find the nucleation site
    for (i = 0; i < NumNucSiteAllD ; i++){		
        R1 = cylinder->NucSite_R[i]; 
        R2 = cylinder->NucSite_R[i+1];

        if (randVal > R1){
            if (randVal < R2){
		        cylinder->NucSite_F[i+1] = 1;
            }
        }
    }

    // Save the data for nucleation probability in a file(kMC_NucSite3)
    if (DEBUG_kMC){
        snprintf(fileName, sizeof(fileName), "%s/kMC_NucSite3",DIR_PROPERTIES);
        fp = fopen(fileName, "a");
        fprintf(fp, "*** cycle=%-8d, # of Nuc=%d, deltaT =%e ,randVal=%f\n", home->cycle, param->NucNum,dtt,randVal);
        fprintf(fp, "Site#\tSCF\t\t\t\tP\t\t\t\tR\t\t\tF\tNucSite_Num\n");
        for (k = 0; k < NumNucSiteAllD ; k++){
            fprintf(fp,"%d\t %e\t %e\t %e\t %d\t\t %d\n",
            k,cylinder->NucSite_SCF[k], 
            cylinder->NucSite_P[k],cylinder->NucSite_R[k],cylinder->NucSite_F[k],cylinder->NucSite_Num[k]);
        }
        fclose(fp);
    }

    return;
}

/*---------------------------------------------------------------------------
 *
 *	Function:	Find_Slip_System
 *	Description:This function is to find the slip system which has 
 *  			max. RSS on the node which located at 
 *	    		the loop center.
 *	Algorithm:  Slip system is chosen based on the maximum resolved 	
 *	    		shear stress.If several maximums exist, choose one in 
 *		    	a random way.  
 *-------------------------------------------------------------------------*/

int Find_Slip_System(real8 NBmatrix[12][6],real8 SS[3][3],real8 c[3],real8 N[3],real8 B[3])
{
    int     i,j,k,l,temp;
    int     NumSlipSystems = 12;
    int     MaxIndex;
    int     RandSequence[NumSlipSystems];
    real8	bx, by, bz, bmag;
    real8	nx, ny, nz, nmag;
    real8	SlipB[3], SlipN[3];
    real8	x0, y0, z0;		// a point at the center 
    real8	x1, y1, z1;		// a Point at surface 
    real8	x2, y2, z2;		// a Neighbor Point in tangent direction
    real8	X01[3];			// a Vector from X0 to X1
    real8	SSB[3], T[3], RSS[NumSlipSystems];
    real8	PK[3], PK_Norm[NumSlipSystems];			
    real8	tol = 1e-3;  	 
    real8	tol2 = 1e-3;  	 
    real8	max = 0.0;  	 
    x1 = c[0];     y1 = c[1];     z1 = c[2];

/*
 *  Compute RSS on the slip system : Sigma[n][b] = DotProduct(N,SS*b)
 */
    for (i = 0; i < NumSlipSystems ; i++){
        SlipN[0] = NBmatrix[i][0]; SlipN[1] = NBmatrix[i][1]; SlipN[2] = NBmatrix[i][2];
        SlipB[0] = NBmatrix[i][3]; SlipB[1] = NBmatrix[i][4]; SlipB[2] = NBmatrix[i][5];

        for (j = 0; j < 3; j++) {
            SSB[j] = SS[j][0]*SlipB[0] + SS[j][1]*SlipB[1] + SS[j][2]*SlipB[2];
        }
	    RSS[i] = DotProduct(SlipN,SSB);

        if (DEBUG_PRINT1){
            printf("SlipN = [%.4f %.4f %.4f];\n", SlipN[0], SlipN[1],SlipN[2]);
            printf("SlipB = [%.4f %.4f %.4f];\n", SlipB[0], SlipB[1],SlipB[2]);
            printf("SSB = [%e %e %e];\n",SSB[0],SSB[1],SSB[2]);
            printf("RSS1(%d) = %e;\n",i+1, RSS[i]); 
        }
    }

/*
 *  Randomize the squence of slip system
 */
    for (i = 0; i < NumSlipSystems ; i++){
        RandSequence[i]=i;
    }

    for (i = 0; i < NumSlipSystems ; i++){
        k = (rand()%11)+1;
        temp = RandSequence[i];
        RandSequence[i]=RandSequence[k];
        RandSequence[k]=temp;
    }

    if (DEBUG_PRINT1){
        printf("Random Sequence\n");
        for (i = 0; i < NumSlipSystems ; i++){
            printf("  %d",RandSequence[i]);
        }
        printf("\n");
    }

/*
 *  Determine the slip system which has max.RSS.
 *  IF there are several maximums, choose the last one.
 *  Since we randomized the sequence, it actally choose one of the maximum in a random way.
 */

    if (DEBUG_PRINT1)     printf("\nk  i  RSS     Max      MaxIndex\n" );

    for (k = 0; k < NumSlipSystems ; k++){
        i = RandSequence[k]; 
        if (RSS[i] >= max){
            max = RSS[i];
            MaxIndex = i;
        }
        if (DEBUG_PRINT1) {
            printf("%d  %d  %e  %e  %d\n",k, i,RSS[i],max,MaxIndex );
            printf("Resolved shear stress= %e \n", RSS[i]);
        }
    }

    nx = NBmatrix[MaxIndex][0];	ny = NBmatrix[MaxIndex][1];	nz = NBmatrix[MaxIndex][2];
    bx = NBmatrix[MaxIndex][3];	by = NBmatrix[MaxIndex][4];	bz = NBmatrix[MaxIndex][5];

    if (DEBUG_PRINT1){
        printf("Sellected Slip System\n");
        printf("nx=%e;ny=%e;nz=%e;bx=%e;by=%e;bz=%e;\n",nx, ny,nz,bx,by,bz);
    }
    B[0] = bx;     B[1] = by;     B[2] = bz;
    N[0] = nx;     N[1] = ny;     N[2] = nz;
    return(MaxIndex);
}

/*---------------------------------------------------------------------------
 *
 *	Function:	LOOPGENERATE_FCC
 *	Description:	This function generate a dislocation loop at the surface 
 *			in FCC metal 
 *
 *	NOTE:		Make a dislocation loop at the ith Nucleation site where
 *			cylinder->NucSite_F[i] is equal to "1"
 *			
 *-------------------------------------------------------------------------*/

void LOOPGENERATE_FCC(Home_t *home, Cylinder_t *cylinder)
{
    Param_t *param;
    param = home->param;

    /* # of nulceation site is set proportional to the diameter 
	 * 	e.g) D=150nm  NumNucSiteD = 150 
	 * 	e.g) D=1000nm NumNucSiteD = 1000 */
	int     NumNucSiteD = (int) (param->cyl_radius*1.0*param->burgMag*1e9+1.0);

#ifdef _BOUNDARY
     /* To allow nulceations on GB, the total number of nucleation site is increase by 1/PI */ 
    int NumNucSiteBoundD = (int) (NumNucSiteD/PI);
    int NumNucSiteAllD = NumNucSiteD + NumNucSiteBoundD;
#else
    int NumNucSiteAllD = NumNucSiteD;
#endif

	real8	mu = param->shearModulus;
	real8   LoopR = param->LoopRadius;

	real8	bx, by, bz, bmag, BB[3];
	real8	nx, ny, nz, nmag, NN[3];
	real8	cx, cy, cz, c[3];				// Center of a dislocation loop
	real8	e1x, e1y, e1z;
	real8	e2x, e2y, e2z;
	real8	fx, fy, fz;
	real8	Rdir;
	real8	Ax,Ay,Az;
	real8	Bx,By,Bz;
	real8	alpha;
	real8	sweptArea;
    real8   dyad[3][3], dstn[3][3], dspn[3][3];
    Node_t  *node;
    int     i, j, l, newNodeKeyPtr;
    int     iArm;
    int     globalOp = 1;
	real8	randnum, u, Rn, uRn, R1, R2;
	real8   theta;
	real8 	segsize = home->param->minSeg;
	int	    NumNode = (int)(2.0*PI*LoopR/segsize);
	int 	SlipIndex;
	real8	SS[3][3], RSS;
	real8	nbmatrix[12][6];	// slip system in {001} coord
	real8	NBmatrix[12][6];	// slip system in the rotated coord w.r.t. cylinder orientation
	real8	Eiej[3][3], e1[3], e2[3], e3[3], E1[3], E2[3], E3[3];
	int	    maxi,maxj;
	real8	n1[3], n2[3], n3[3], n4[3];
	real8	b11[3],b12[3],b13[3];
	real8	b21[3],b22[3],b23[3];
	real8	b31[3],b32[3],b33[3];
	real8	b41[3],b42[3],b43[3];
	real8	radius =param->cyl_radius; 
	real8	ShortestR = radius;
	int	    ShortestR_I;
	real8	LoopNode_R;
	real8	SSB[3], Xi[3], PKLoop[3];
	real8	dS,ep_slip;
	real8	S = 1.0 ;
	real8   Schmid = sqrt(6)/6.0;             	// Schmid factor in FCC
	real8	Young  = 2*mu*(1.0 + param->pois);	// Young's modulus
	real8	beta   = 22.6375*M_PI/180;          // Angle between slip plane and loading direction[rad]
	real8	L0     = 10.0*radius;
	int	Slip;					// Number of slip per sites	
	real8	al, am, an, amag, sigijk; 

#if defined _CYLINDER && _TORSION /*iryu*/
	real8	ThetaEla;
    real8   AvgDistance  , Ip;
    real8   br, bq, nr,nq;
    real8   dpTheta_Nuc;
#endif

    /* Generate dislocation loop on the free surface */

	for (l = 0; l < NumNucSiteAllD ; l++){
#ifdef PARALLEL /*iryu*/
        if (home->myDomain != 0) continue;
#endif
        if (cylinder->NucSite_F[l]){ // Find the nucleation site

			if (NumNode<5) NumNode = 5;		// minimum number of node in the loop
			if (NumNode>20) NumNode = 20;	// maximum number of node in the loop

			real8	N =(float) NumNode;	
			Node_t  *LoopNode[NumNode];

			// Loop center 
		 	cx = cylinder->NucSite_x[l];
			cy = cylinder->NucSite_y[l];
			cz = cylinder->NucSite_z[l];

			for (i = 0; i < NumNode; i++) {
				LoopNode[i] = GetNewNativeNode(home); 
				if (DEBUG_PRINT) printf("GetNewNativeNode : %d\n",i);
				FreeNodeArms(LoopNode[i]);
				if (DEBUG_PRINT) printf("FreeNodeArms\n");
				LoopNode[i]->native = 1;
				LoopNode[i]->numNbrs= 0;
				if (DEBUG_PRINT) 	printf("native, numNbrs\n");
				LoopNode[i]->constraint = UNCONSTRAINED;
				if (DEBUG_PRINT) 	printf("Node->constarint\n");
				LoopNode[i]->oldvX = 0.0;
				LoopNode[i]->oldvY = 0.0;
				LoopNode[i]->oldvZ = 0.0;
				if (DEBUG_PRINT) 	printf("Node->oldvXYZ\n");
			}

			SS[0][0] = param->appliedStress[0]; 
			SS[1][1] = param->appliedStress[1];  
			SS[2][2] = param->appliedStress[2];
			SS[1][2] = param->appliedStress[3];
			SS[0][2] = param->appliedStress[4];
			SS[0][1] = param->appliedStress[5];

#if defined _CYLINDER && _TORSION /*iryu*/
			ThetaEla = param->AppliedTheta*M_PI/180;

			/* Pure torsion + applied stress in Z direction */
			SS[1][2] = SS[1][2] + cx*mu*ThetaEla;
			SS[0][2] = SS[0][2] - cy*mu*ThetaEla;
#endif
			SS[2][0] = SS[0][2];
			SS[2][1] = SS[1][2];
			SS[1][0] = SS[0][1];
        
			/* All primary slip system in FCC (Reference coordinate) */
			n1[0] = 1/sqrt(3.0);	n1[1] = 1/sqrt(3.0);	n1[2] = 1/sqrt(3.0);
			b11[0]= 1/sqrt(2.0);	b11[1]=-1/sqrt(2.0);	b11[2]= 0.0;	
			b12[0]= 1/sqrt(2.0);	b12[1]= 0.0;	    	b12[2]=-1/sqrt(2.0);	
			b13[0]= 0.0;    		b13[1]= 1/sqrt(2.0);	b13[2]=-1/sqrt(2.0);	
			
			n2[0] =-1/sqrt(3.0);	n2[1] = 1/sqrt(3.0);	n2[2] = 1/sqrt(3.0);
			b21[0]= 1/sqrt(2.0);	b21[1]= 0.0;		    b21[2]= 1/sqrt(2.0);	
			b22[0]= 1/sqrt(2.0);	b22[1]= 1/sqrt(2.0);	b22[2]= 0.0;	
			b23[0]= 0.0;	    	b23[1]=-1/sqrt(2.0);	b23[2]= 1/sqrt(2.0);	

			n3[0] = 1/sqrt(3.0);	n3[1] =-1/sqrt(3.0);	n3[2] = 1/sqrt(3.0);
			b31[0]=-1/sqrt(2.0);	b31[1]= 0.0;		    b31[2]= 1/sqrt(2.0);	
			b32[0]= 0.0;	    	b32[1]= 1/sqrt(2.0);	b32[2]= 1/sqrt(2.0);	
			b33[0]= 1/sqrt(2.0);	b33[1]= 1/sqrt(2.0);	b33[2]= 0.0;	

			n4[0] = 1/sqrt(3.0);	n4[1] = 1/sqrt(3.0);	n4[2] =-1/sqrt(3.0);
			b41[0]= 1/sqrt(2.0);	b41[1]= 0.0;		    b41[2]= 1/sqrt(2.0);	
			b42[0]= 0.0;	    	b42[1]= 1/sqrt(2.0);	b42[2]= 1/sqrt(2.0);	
			b43[0]=-1/sqrt(2.0);	b43[1]= 1/sqrt(2.0);	b43[2]= 0.0;	

			/*	Slip system in {001} coordinate system 
			 *	nbmatrix[12][0-2] : slip normal
			 *	nbmatrix[12][3-5] : Burgers vector */

			nbmatrix[0][0]=n1[0];	nbmatrix[0][1]=n1[1];	nbmatrix[0][2]=n1[2]; 
			nbmatrix[1][0]=n1[0];	nbmatrix[1][1]=n1[1];	nbmatrix[1][2]=n1[2]; 
			nbmatrix[2][0]=n1[0];	nbmatrix[2][1]=n1[1];	nbmatrix[2][2]=n1[2]; 

			nbmatrix[3][0]=n2[0];	nbmatrix[3][1]=n2[1];	nbmatrix[3][2]=n2[2]; 
			nbmatrix[4][0]=n2[0];	nbmatrix[4][1]=n2[1];	nbmatrix[4][2]=n2[2]; 
			nbmatrix[5][0]=n2[0];	nbmatrix[5][1]=n2[1];	nbmatrix[5][2]=n2[2]; 

			nbmatrix[6][0]=n3[0];	nbmatrix[6][1]=n3[1];	nbmatrix[6][2]=n3[2]; 
			nbmatrix[7][0]=n3[0];	nbmatrix[7][1]=n3[1];	nbmatrix[7][2]=n3[2]; 
			nbmatrix[8][0]=n3[0];	nbmatrix[8][1]=n3[1];	nbmatrix[8][2]=n3[2]; 

			nbmatrix[9][0]=n4[0];	nbmatrix[9][1]=n4[1];	nbmatrix[9][2]=n4[2]; 
			nbmatrix[10][0]=n4[0];	nbmatrix[10][1]=n4[1];	nbmatrix[10][2]=n4[2]; 
			nbmatrix[11][0]=n4[0];	nbmatrix[11][1]=n4[1];	nbmatrix[11][2]=n4[2]; 

			nbmatrix[0][3]=b11[0];	nbmatrix[0][4]=b11[1];	nbmatrix[0][5]=b11[2];	
			nbmatrix[1][3]=b12[0];	nbmatrix[1][4]=b12[1];	nbmatrix[1][5]=b12[2];	
			nbmatrix[2][3]=b13[0];	nbmatrix[2][4]=b13[1];	nbmatrix[2][5]=b13[2];	

			nbmatrix[3][3]=b21[0];	nbmatrix[3][4]=b21[1];	nbmatrix[3][5]=b21[2];	
			nbmatrix[4][3]=b22[0];	nbmatrix[4][4]=b22[1];	nbmatrix[4][5]=b22[2];	
			nbmatrix[5][3]=b23[0];	nbmatrix[5][4]=b23[1];	nbmatrix[5][5]=b23[2];	

			nbmatrix[6][3]=b31[0];	nbmatrix[6][4]=b31[1];	nbmatrix[6][5]=b31[2];	
			nbmatrix[7][3]=b32[0];	nbmatrix[7][4]=b32[1];	nbmatrix[7][5]=b32[2];	
			nbmatrix[8][3]=b33[0];	nbmatrix[8][4]=b33[1];	nbmatrix[8][5]=b33[2];	

			nbmatrix[9][3]=b41[0];	nbmatrix[9][4]=b41[1];	nbmatrix[9][5]=b41[2];	
			nbmatrix[10][3]=b42[0];	nbmatrix[10][4]=b42[1];	nbmatrix[10][5]=b42[2];	
			nbmatrix[11][3]=b43[0];	nbmatrix[11][4]=b43[1];	nbmatrix[11][5]=b43[2];	

			e1[0] = 1.0;	e1[1] = 0.0;	e1[2] = 0.0;
			e2[0] = 0.0;	e2[1] = 1.0;	e2[2] = 0.0;
			e3[0] = 0.0;	e3[1] = 0.0;	e3[2] = 1.0;

			E1[0] = 1.0;	E1[1] = 0.0;	E1[2] = 0.0;
			E2[0] = 0.0;	E2[1] = 1.0;	E2[2] = 0.0;
			E3[0] = 0.0;	E3[1] = 0.0;	E3[2] = 1.0;

			if(param->ANISO_110){
			/* vector transformation matrix (101) -> (001)
			 * E1 = [1 0 -1], E2 = [0 1 0], E3 = [1 0 1]  */
				E1[0] = 1.0/sqrt(2.0);	E1[1] = 0.0;		E1[2] =-1.0/sqrt(2.0);
				E2[0] = 0.0;		E2[1] = 1.0;		E2[2] = 0.0;
				E3[0] = 1.0/sqrt(2.0);	E1[1] = 0.0;		E3[2] = 1.0/sqrt(2.0);
			}

			if(param->ANISO_111){
			/* vector transformation matrix (111) -> (001)
			 * E1 = [1 -1 0] , E3 = [1 1 1] E2 = cross(E3,E1) */
				E1[0] = 1.0/sqrt(2.0);	E1[1] =-1.0/sqrt(2.0);	E1[2] = 0.0;
				E2[0] = 1.0/sqrt(6.0);	E2[1] = 1.0/sqrt(6.0);	E2[2] =-2.0/sqrt(6.0);
				E3[0] = 1.0/sqrt(3.0);	E3[1] = 1.0/sqrt(3.0);	E3[2] = 1.0/sqrt(3.0);
			}

			// Transformattion Metric(DotProduct(Ei,ej))	
			Eiej[0][0] = DotProduct(E1,e1); Eiej[0][1] = DotProduct(E1,e2); Eiej[0][2] = DotProduct(E1,e3);
			Eiej[1][0] = DotProduct(E2,e1); Eiej[1][1] = DotProduct(E2,e2); Eiej[1][2] = DotProduct(E2,e3);
			Eiej[2][0] = DotProduct(E3,e1); Eiej[2][1] = DotProduct(E3,e2); Eiej[2][2] = DotProduct(E3,e3);


			if (DEBUG_PRINT1){
				printf("Eiej1 = [\n");
				printf("\t%.4f %.4f %.4f ;\n", Eiej[0][0],Eiej[0][1],Eiej[0][2]);
				printf("\t%.4f %.4f %.4f ;\n", Eiej[1][0],Eiej[1][1],Eiej[1][2]);
				printf("\t%.4f %.4f %.4f ];\n",Eiej[2][0],Eiej[2][1],Eiej[2][2]);
			}

			/* transformation from (001) coord to the rotated coord */
		  	for (i = 0; i < 12; i++) {
 			    NBmatrix[i][0] = Eiej[0][0]*nbmatrix[i][0] + Eiej[0][1]*nbmatrix[i][1] + Eiej[0][2]*nbmatrix[i][2];
			    NBmatrix[i][1] = Eiej[1][0]*nbmatrix[i][0] + Eiej[1][1]*nbmatrix[i][1] + Eiej[1][2]*nbmatrix[i][2];
 			    NBmatrix[i][2] = Eiej[2][0]*nbmatrix[i][0] + Eiej[2][1]*nbmatrix[i][1] + Eiej[2][2]*nbmatrix[i][2];
 	
			    NBmatrix[i][3] = Eiej[0][0]*nbmatrix[i][3] + Eiej[0][1]*nbmatrix[i][4] + Eiej[0][2]*nbmatrix[i][5];
			    NBmatrix[i][4] = Eiej[1][0]*nbmatrix[i][3] + Eiej[1][1]*nbmatrix[i][4] + Eiej[1][2]*nbmatrix[i][5];
  			    NBmatrix[i][5] = Eiej[2][0]*nbmatrix[i][3] + Eiej[2][1]*nbmatrix[i][4] + Eiej[2][2]*nbmatrix[i][5];
			}

			c[0] = cx;	c[1] = cy;	c[2] = cz;

			if (DEBUG_PRINT1){
				printf("nbmatrix1 = [\n");
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[0][0],nbmatrix[0][1],nbmatrix[0][2],	
									  nbmatrix[0][3],nbmatrix[0][4],nbmatrix[0][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[1][0],nbmatrix[1][1],nbmatrix[1][2],
									  nbmatrix[1][3],nbmatrix[1][4],nbmatrix[1][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[2][0],nbmatrix[2][1],nbmatrix[2][2],
									  nbmatrix[2][3],nbmatrix[2][4],nbmatrix[2][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[3][0],nbmatrix[3][1],nbmatrix[3][2],
									  nbmatrix[3][3],nbmatrix[3][4],nbmatrix[3][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[4][0],nbmatrix[4][1],nbmatrix[4][2],
									  nbmatrix[4][3],nbmatrix[4][4],nbmatrix[4][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[5][0],nbmatrix[5][1],nbmatrix[5][2],
									  nbmatrix[5][3],nbmatrix[5][4],nbmatrix[5][5]);

				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[6][0],nbmatrix[6][1],nbmatrix[6][2],
									  nbmatrix[6][3],nbmatrix[6][4],nbmatrix[6][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[7][0],nbmatrix[7][1],nbmatrix[7][2],
									  nbmatrix[7][3],nbmatrix[7][4],nbmatrix[7][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[8][0],nbmatrix[8][1],nbmatrix[8][2],
									  nbmatrix[8][3],nbmatrix[8][4],nbmatrix[8][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[9][0],nbmatrix[9][1],nbmatrix[9][2],
									  nbmatrix[9][3],nbmatrix[9][4],nbmatrix[9][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ;\n",nbmatrix[10][0],nbmatrix[10][1],nbmatrix[10][2],
									  nbmatrix[10][3],nbmatrix[10][4],nbmatrix[10][5]);
				printf("%.4f %.4f %.4f %.4f %.4f %.4f ];\n",nbmatrix[11][0],nbmatrix[11][1],nbmatrix[11][2],
									  nbmatrix[11][3],nbmatrix[11][4],nbmatrix[11][5]);
				printf("NBmatrix1 = [\n");
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[0][0],NBmatrix[0][1],NBmatrix[0][2],	
									  NBmatrix[0][3],NBmatrix[0][4],NBmatrix[0][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[1][0],NBmatrix[1][1],NBmatrix[1][2],
									  NBmatrix[1][3],NBmatrix[1][4],NBmatrix[1][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[2][0],NBmatrix[2][1],NBmatrix[2][2],
									  NBmatrix[2][3],NBmatrix[2][4],NBmatrix[2][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[3][0],NBmatrix[3][1],NBmatrix[3][2],
									  NBmatrix[3][3],NBmatrix[3][4],NBmatrix[3][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[4][0],NBmatrix[4][1],NBmatrix[4][2],
									  NBmatrix[4][3],NBmatrix[4][4],NBmatrix[4][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[5][0],NBmatrix[5][1],NBmatrix[5][2],
									  NBmatrix[5][3],NBmatrix[5][4],NBmatrix[5][5]);

				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[6][0],NBmatrix[6][1],NBmatrix[6][2],
									  NBmatrix[6][3],NBmatrix[6][4],NBmatrix[6][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[7][0],NBmatrix[7][1],NBmatrix[7][2],
									  NBmatrix[7][3],NBmatrix[7][4],NBmatrix[7][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[8][0],NBmatrix[8][1],NBmatrix[8][2],
									  NBmatrix[8][3],NBmatrix[8][4],NBmatrix[8][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[9][0],NBmatrix[9][1],NBmatrix[9][2],
									  NBmatrix[9][3],NBmatrix[9][4],NBmatrix[9][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ;\n",NBmatrix[10][0],NBmatrix[10][1],NBmatrix[10][2],
									  NBmatrix[10][3],NBmatrix[10][4],NBmatrix[10][5]);
				printf("\t%.4f %.4f %.4f %.4f %.4f %.4f ];\n",NBmatrix[11][0],NBmatrix[11][1],NBmatrix[11][2],
									  NBmatrix[11][3],NBmatrix[11][4],NBmatrix[11][5]);
				printf("c0 = [%e %e %e ];\n",c[0],c[1],c[2]);

			}

			SlipIndex = Find_Slip_System(NBmatrix, SS, c, NN, BB);

			bx = BB[0];	by = BB[1];	bz = BB[2];
			nx = NN[0];	ny = NN[1];	nz = NN[2];

			param->Slip_System[SlipIndex] ++; 

			// Base vectors to draw a loop 
			e1x = bx;		e1y = by;		e1z = bz;
			e2x = by*nz - bz*ny;	e2y = bz*nx - bx*nz;	e2z = bx*ny - by*nx;

			// Position of nodes on the dislocation loop
			for (i = 0; i < NumNode; i++) {
				theta = (float) i;
				theta = (2*PI/N)*theta;
				LoopNode[i]->x = cx + LoopR*(e1x*cos(theta) + e2x*sin(theta));
				LoopNode[i]->y = cy + LoopR*(e1y*cos(theta) + e2y*sin(theta));
				LoopNode[i]->z = cz + LoopR*(e1z*cos(theta) + e2z*sin(theta));
			}

/*           Check (2017/7/16) 

			// Make sure that a nucleated dislocation loop would expand outward from the loop center 

			for (j = 0; j < 3; j++) {
			     SSB[j] = SS[j][0]*bx + SS[j][1]*by + SS[j][2]*bz;
			}

			Xi[0] = LoopNode[2]->x - LoopNode[0]->x; 
			Xi[1] = LoopNode[2]->y - LoopNode[0]->y; 
			Xi[2] = LoopNode[2]->z - LoopNode[0]->z; 

			PKLoop[0]=SSB[1]*Xi[2]-SSB[2]*Xi[1];
			PKLoop[1]=SSB[2]*Xi[0]-SSB[0]*Xi[2];
			PKLoop[2]=SSB[0]*Xi[1]-SSB[1]*Xi[0];

			fx = PKLoop[0];
			fy = PKLoop[1]; 
			fz = PKLoop[2]; 
	
			Rdir = (LoopNode[1]->x-cx)*fx + 
                   (LoopNode[1]->y-cy)*fy + 
                   (LoopNode[1]->z-cz)*fz;
			if (Rdir < 0.0){ 
                // P-K Force does not point outward from the center of the loop,
                // flip the Burgers vector
				bx =-1.0*bx;    by =-1.0*by;    bz =-1.0*bz;
			}
*/

/*
 * To add plastic strain/twist from the dislocation nucleation 
 */

			Ax = LoopNode[0]->x - cx;
			Ay = LoopNode[0]->y - cy;
			Az = LoopNode[0]->z - cz;

			Bx = LoopNode[NumNode-1]->x - cx;
			By = LoopNode[NumNode-1]->y - cy;
			Bz = LoopNode[NumNode-1]->z - cz;

			alpha = acos(((Ax*Bx)+(Ay*By)+(Az*Bz))/(sqrt(Ax*Ax+Ay*Ay+Az*Az)*sqrt(Bx*Bx+By*By+Bz*Bz)));
			sweptArea = 0.5*LoopR*LoopR*alpha;

#if defined _TORSION 
            /*  For torsion, add plastic twist  */	
			AvgDistance = 0.5*(radius+(radius-LoopR));
			Ip = 0.5*M_PI*radius*radius*radius*radius;

			// Slip system in cylindrical coordinate system
			br =  bx*(cx/radius) + by*(cy/radius);
			bq = -bx*(cy/radius) + by*(cx/radius);
			nr =  nx*(cx/radius) + ny*(cy/radius);
			nq = -nx*(cy/radius) + ny*(cx/radius);

			// plastic twist due to the nucleation
			dpTheta_Nuc = AvgDistance/(L0*Ip)*(bz*nq + bq*nz)*sweptArea;
/*
 *  To make sure that the plastic twist increment due to nucleation is positive under torsion
 */	
#if defined _TENSIONafterTORSION 
            //  Do Nothing
#else
			if (dpTheta_Nuc*param->Loading_Direction < 0.0){	// Revert Burgers Vector
				bx =-1.0*bx;    by =-1.0*by;    bz =-1.0*bz;
				dpTheta_Nuc = -1.0*dpTheta_Nuc;
			}
#endif
			param->pTheta = param->pTheta + dpTheta_Nuc ;
			param->DelpTheta = param->DelpTheta + dpTheta_Nuc ;
#endif
            /* For tension, add plastic strain */	
			dyad[0][0] = nx*bx; dyad[0][1] = nx*by; dyad[0][2] = nx*bz;
			dyad[1][0] = ny*bx; dyad[1][1] = ny*by; dyad[1][2] = ny*bz;
			dyad[2][0] = nz*bx; dyad[2][1] = nz*by; dyad[2][2] = nz*bz;

			for (i = 0; i < 3; i++){
			    for (j = 0; j < 3; j++){
			        dstn[j][i] = (dyad[j][i]+dyad[i][j])*sweptArea/(2.0*param->simVol);
			        dspn[j][i] = (dyad[j][i]-dyad[i][j])*sweptArea/(2.0*param->simVol); 
			    }
			}
			param->delpStrain[0] += dstn[0][0];
			param->delpStrain[1] += dstn[1][1];
			param->delpStrain[2] += dstn[2][2];
			param->delpStrain[3] += dstn[1][2];
			param->delpStrain[4] += dstn[0][2];
			param->delpStrain[5] += dstn[0][1];

			param->delpSpin[0] += dspn[0][0];
			param->delpSpin[1] += dspn[1][1];
			param->delpSpin[2] += dspn[2][2];
			param->delpSpin[3] += dspn[1][2];
			param->delpSpin[4] += dspn[0][2];
			param->delpSpin[5] += dspn[0][1];

/*
 *  Make a dislocation loop 
 */
			for (i = 1; i < NumNode-1; i++) {
				InsertArm(home, LoopNode[i], &LoopNode[i+1]->myTag,     bx,     by,     bz, nx, ny, nz, globalOp);
				InsertArm(home, LoopNode[i], &LoopNode[i-1]->myTag,-1.0*bx,-1.0*by,-1.0*bz, nx, ny, nz, globalOp);
			}
			InsertArm(home, LoopNode[0], &LoopNode[1]->myTag,             bx,     by,     bz, nx, ny, nz, globalOp);
			InsertArm(home, LoopNode[0], &LoopNode[NumNode-1]->myTag,-1.0*bx,-1.0*by,-1.0*bz, nx, ny, nz, globalOp);

			InsertArm(home, LoopNode[NumNode-1], &LoopNode[0]->myTag,     bx,     by,     bz, nx, ny, nz, globalOp);
			InsertArm(home, LoopNode[NumNode-1], &LoopNode[NumNode-2]->myTag,-1.0*bx,-1.0*by,-1.0*bz, nx, ny, nz, globalOp);

/* 
 *  Print out slip system of nucleated dislocation 
 */
#ifdef _BOUNDARY
            if (l<NumNucSiteD){
                printf("%dth Nucleation at Site %d : \n c = [%3f %3f %3f] \n b = [%4f,%4f,%.4f],  \n n = [%.4f, %.4f,%4f] \n",
                        param->NucNum+1,l, cx, cy,cz, bx,by,bz, nx,ny,nz);
            }
            else{
                printf("%dth Nucleation at Site %d on GB : \n c = [%3f %3f %3f] \n b = [%4f,%4f,%.4f],  \n n = [%.4f, %.4f,%4f] \n",
                        param->NucNum+1,l, cx, cy,cz, bx,by,bz, nx,ny,nz);
            }

#else
            printf("%dth Nucleation at Site %d : \n c = [%3f %3f %3f] \n b = [%4f,%4f,%.4f],  \n n = [%.4f, %.4f,%4f] \n",
                    param->NucNum+1,l, cx, cy,cz, bx,by,bz, nx,ny,nz);
#endif

			printf("stress = [\n");
			printf("%e %e %e ;\n",SS[0][0],SS[0][1],SS[0][2]);
			printf("%e %e %e ;\n",SS[1][0],SS[1][1],SS[1][2]);
			printf("%e %e %e ];\n",SS[2][0],SS[2][1],SS[2][2]);

			for (j = 0; j < 3; j++) {
			     SSB[j] = SS[j][0]*bx + SS[j][1]*by + SS[j][2]*bz;
			}
			RSS = DotProduct(NN,SSB);
			printf("Effective_resolved_shear_stress=%e     //RSS*SCF \n",RSS*cylinder->NucSite_SCF[l]);
			
/*  
 *  Update number of total nucleation
 */
			param->NucNum ++;
			cylinder->NucSite_Num[l] = cylinder->NucSite_Num[l]+1; 
        }
	}
	return;
}

/*---------------------------------------------------------------------------
 *
 *      Function:     randn
 *      Description:  
 *        1. Generate uniform distribution of U (-1,1)
 *        2. Generate X1 ~ N(0,1)
 *        3. Generate X2 ~ N(mu,sigma)
 *
 *      Returns:  real8 value from X2
 *
 *-------------------------------------------------------------------------*/

double randn (double mu, double sigma)
{
  double U1, U2, W, mult;
  static double X1, X2;
  static int call = 0;
 
  if (call == 1)
    {
      call = !call;
      return (mu + sigma * (double) X2);
    }
 
  do
    {
      U1 = -1 + ((double) rand () / RAND_MAX) * 2;
      U2 = -1 + ((double) rand () / RAND_MAX) * 2;
      W = pow (U1, 2) + pow (U2, 2);
    }
  while (W >= 1 || W == 0);
 
  mult = sqrt ((-2 * log (W)) / W);
  X1 = U1 * mult;
  X2 = U2 * mult;
 
  call = !call;
 
  return (mu + sigma * (double) X1);
}
