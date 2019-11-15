#include <math.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_roots.h>
#include <gsl/gsl_sf_hyperg.h> 
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>

#include <gsl/gsl_odeiv2.h>

#include <fastpm/libfastpm.h>

#define STEF_BOLT 2.851e-48  // in units: [ h * (10^10Msun/h) * s^-3 * K^-4 ]
#define rho_crit 27.7455     //rho_crit0 in mass/length (not energy/length)
#define LIGHT 9.722e-15      // in units: [ h * (Mpc/h) * s^-1 ]      //need to update all these units and change Omega_g

#define kB 8.617330350e-5    //boltzman in eV/K   i th

double HubbleDistance = 2997.92458; /* Mpc/h */   /*this c*1e5 in SI units*/
double HubbleConstant = 100.0; /* Mpc/h / km/s*/     //OTHER WAY ROUND!

double Omega_g(FastPMCosmology * c)
{
    /* This is really Omega_g0. All Omegas in this code mean 0, exceot nu!!, I would change notation, but anyway 
    Omega_g0 = 4 \sigma_B T_{CMB}^4 8 \pi G / (3 c^3 H0^2)*/
    return 4 * STEF_BOLT * pow(c->T_cmb, 4) / pow(LIGHT, 3) / rho_crit / pow(c->h, 2);
}

double Gamma_nu(FastPMCosmology * c)
{
    /* nu to photon temp ratio today */
    if (c->N_nu == 0) {      //avoids nan because N_nu in denom (N_nu=0 => N_eff=0 => Gamma_nu=0)
        return 0.;
    }else{
        return pow(c->N_eff / c->N_nu, 1./4.) * pow( 4./11., 1./3.);
    }
}

double Omega_ur(FastPMCosmology * c)
{
    /*Omega_ur0. This is the energy density of all massless nus.*/
    int N_ur = c->N_nu - c->N_ncdm;    //number of massless nus. Different to CLASS defn
    return 7./8. * N_ur * pow(Gamma_nu(c), 4) * Omega_g(c);
}

double Omega_r(FastPMCosmology * c)
{
    /*Omega_r0. This is the energy density of all radiation-like particles.
      FIXME: really this is Omega_g+ur, not r, because it doesn't have ncdm,r in it 
      and we define m with ncdm,m. this doesn't affect rest of code, but maybe redefine.*/
    return Omega_g(c) + Omega_ur(c);
}

double getFtable(int F_id, double y, FastPMCosmology * c)
{
    //Not using size as an arg, it's globally defined
    //Gets the interpolated value of Ftable[F_id] at y
    //F_id: 1 for F, 2 for F', 3 for F''
    
    //if (y == 0.) {               //does this work for double?
    //    return 0.;                //this hack ensures all Omega_ncdm related funcs will return 0.
    //}else{
    return fastpm_do_fd_interp(c->FDinterp, F_id, y);
    //}
}

double Fconst(int ncdm_id, FastPMCosmology * c)
{
    /*
    This is a cosmology dependent constant which is the argument divided by a of F, DF, DDF used repeatedly in code
    To be clear, evaluate F at Fconst*a.
    Fconst must be calculated for each ncdm species as it depends on the mass. ncdm_id labels the element of m_ncdm.
    */
    
    if (c->T_cmb == 0. || c->N_ncdm == 0) {
        return 0.;          // Incase you want to run with no radaition in bkg! This is a bit ugly... because we still go thru all the F funcs
    }else{
    double T_nu = Gamma_nu(c) * c->T_cmb;
    //printf("%d %g %g %g %g\n", ncdm_id, c->m_ncdm[ncdm_id], c->m_ncdm[ncdm_id] / (kB * T_nu), kB, T_nu);
    return c->m_ncdm[ncdm_id] / (kB * T_nu);
    }
}

double Omega_ncdm_iTimesHubbleEaSq(double a, int ncdm_id, FastPMCosmology * c)   
{
    /* Use interpolation to find Omega_ncdm_i(a) * E(a)^2 */
    
    double A = 15. / pow(M_PI, 4) * pow(Gamma_nu(c), 4) * Omega_g(c);
    double Fc = Fconst(ncdm_id, c);
    double F = getFtable(1, Fc*a, c);              //row 1 for F
    
    return A / (a*a*a*a) * F;
}

double Omega_ncdmTimesHubbleEaSq(double a, FastPMCosmology * c)   
{
    //sum the fermi-dirac integration results for each ncdm species
    double res = 0;
    for (int i=0; i<c->N_ncdm; i++) {
        res += Omega_ncdm_iTimesHubbleEaSq(a, i, c);
    }
    return res;
}

//lots of repn in the below funcs, should we define things globally (seems messy), or object orient?
double DOmega_ncdmTimesHubbleEaSqDa(double a, FastPMCosmology * c)
{
    double A = 15. / pow(M_PI, 4) * pow(Gamma_nu(c), 4) * Omega_g(c);
    
    double OncdmESq = Omega_ncdmTimesHubbleEaSq(a,c);
    
    double FcDF = 0;
    for (int i=0; i<c->N_ncdm; i++) {
        double Fc = Fconst(i, c);
        double DF = getFtable(2, Fc*a, c);
        FcDF += Fc * DF;    //row 2 for F'
    }
    
    return -4. / a * OncdmESq + A / (a*a*a*a) * FcDF;
}

double D2Omega_ncdmTimesHubbleEaSqDa2(double a, FastPMCosmology * c)
{
    double A = 15. / pow(M_PI, 4) * pow(Gamma_nu(c), 4) * Omega_g(c);
    
    double OncdmESq = Omega_ncdmTimesHubbleEaSq(a,c);
    double DOncdmESqDa = DOmega_ncdmTimesHubbleEaSqDa(a,c);
    
    double FcFcDDF = 0;
    for (int i=0; i<c->N_ncdm; i++) {
        double Fc = Fconst(i, c);
        double DDF = getFtable(3, Fc*a, c);
        FcFcDDF += Fc * Fc * DDF;    //row 3 for F''
    }
    
    return -12. / (a*a) * OncdmESq - 8. / a * DOncdmESqDa + A / (a*a*a*a) * FcFcDDF;
}

double w_ncdm_i(double a, int ncdm_id, FastPMCosmology * c)
{
    /*eos parameter for ith neutrino species*/
    double y = Fconst(ncdm_id, c) * a;
    return 1./3. - y / 3. * getFtable(2, y, c) / getFtable(1, y, c);
}

double Omega_Lambda(FastPMCosmology* c)
{
    /*Define Omega_Lambda using z=0 values to give 0 curvature.*/
    double res = 1;
    res -= c->Omega_cdm;
    res -= Omega_r(c);
    res -= Omega_ncdmTimesHubbleEaSq(1, c);
    return res;
}

double HubbleEa(double a, FastPMCosmology * c)
{
    /* H(a) / H0 */
    return sqrt(Omega_r(c) / (a*a*a*a) + c->Omega_cdm / (a*a*a) + Omega_ncdmTimesHubbleEaSq(a, c) + Omega_Lambda(c));
}

double Omega_ncdm_i(double a, int ncdm_id, FastPMCosmology * c)
{
    double E = HubbleEa(a, c);
    return Omega_ncdm_iTimesHubbleEaSq(a, ncdm_id, c) / (E*E);
}

double Omega_ncdm(double a, FastPMCosmology * c)
{
    /*total ncdm*/
    double res = 0;
    for (int i=0; i<c->N_ncdm; i++) {
        res += Omega_ncdm_i(a, i, c);
    }
    return res;
}

double Omega_ncdm_i_m(double a, int ncdm_id, FastPMCosmology * c)
{
    /*matter-like part of ncdm_i*/
    //printf("\n wwww %g %g \n", a, w_ncdm_i(a, ncdm_id, c));
    return (1. - 3 * w_ncdm_i(a, ncdm_id, c)) * Omega_ncdm_i(a, ncdm_id, c);
}

double Omega_ncdm_m(double a, FastPMCosmology * c)
{
    /*total ncdm matter-like part*/
    double res = 0;
    for (int i=0; i<c->N_ncdm; i++) {
        res += Omega_ncdm_i_m(a, i, c);
    }
    return res;
}

double Omega_cdm_a(double a, FastPMCosmology * c)
{
    //as a func of a. I dont like this. I would use 0s in struct instead. ask yu.
    double E = HubbleEa(a, c);
    return c->Omega_cdm / (a*a*a) / (E*E);
}

double OmegaA(double a, FastPMCosmology * c) {
    //this is what I called Omega_cdm_a above. choose which to use later.
    return Omega_cdm_a(a, c);
}

double Omega_m(double a, FastPMCosmology * c){
    /*Total matter component (cdm + ncdm_m)*/
    return Omega_cdm_a(a, c) + Omega_ncdm_m(a, c);
}

double DHubbleEaDa(double a, FastPMCosmology * c)
{
    /* d E / d a*/
    double E = HubbleEa(a, c);
    double DOncdmESqDa = DOmega_ncdmTimesHubbleEaSqDa(a,c);
    
    return 0.5 / E * ( - 4 * Omega_r(c) / pow(a,5) - 3 * c->Omega_cdm / pow(a,4) + DOncdmESqDa );
}

double D2HubbleEaDa2(double a, FastPMCosmology * c)
{
    double E = HubbleEa(a,c);
    double dEda = DHubbleEaDa(a,c);
    double D2OncdmESqDa2 = D2Omega_ncdmTimesHubbleEaSqDa2(a,c);

    return 0.5 / E * ( 20 * Omega_r(c) / pow(a,6) + 12 * c->Omega_cdm / pow(a,5) + D2OncdmESqDa2 - 2 * pow(dEda,2) );
}

double OmegaSum(double a, FastPMCosmology* c)
{
    //should always equal 1. good for testing.
    double sum = Omega_r(c) / pow(a, 4);
    sum += c->Omega_cdm / pow(a, 3);
    sum += Omega_ncdmTimesHubbleEaSq(a, c);
    sum += Omega_Lambda(c);
    return sum / pow(HubbleEa(a, c), 2);
}

static int growth_ode(double a, const double y[], double dyda[], void *params)
{
    //is yy needed?
    FastPMCosmology* c = (FastPMCosmology * ) params;
    
    const double E = HubbleEa(a, c);
    const double dEda = DHubbleEaDa(a, c);
    
    double dydlna[4];
    dydlna[0] = y[1];
    dydlna[1] = - (2. + a / E * dEda) * y[1] + 1.5 * Omega_m(a, c) * y[0];
    dydlna[2] = y[3];
    dydlna[3] = - (2. + a / E * dEda) * y[3] + 1.5 * Omega_m(a, c) * (y[2] - y[0]*y[0]);
    
    //divide by  a to get dyda
    for (int i=0; i<4; i++){
        dyda[i] = dydlna[i] / a;
    }
    
    return GSL_SUCCESS;
}

static ode_soln growth_ode_solve(double a, FastPMCosmology * c)
{
    /* NOTE that the analytic COLA growthDtemp() is 6 * pow(1 - c.OmegaM, 1.5) times growth() */
    /* This returns an array of {d1, F1, d2, F2}
        Is there a nicer way to do this within one func and no object?*/
    gsl_odeiv2_system F;
    F.function = &growth_ode;
    F.jacobian = NULL;
    F.dimension = 4;
    F.params = (void*) c;
    
    gsl_odeiv2_driver * drive 
        = gsl_odeiv2_driver_alloc_standard_new(&F,
                                               gsl_odeiv2_step_rkf45, 
                                               1e-6,
                                               1e-8,
                                               1e-8,
                                               1,
                                               1);
    
     /* We start early to avoid lambda, but still MD so we use y<<1 Meszearos soln. And assume all nus are radiation*/
    
    //Note the normalisation of D is arbitrary as we will only use it to calcualte growth fractor.
    
    // ASSUME MD, no FS
    //double aini = 4e-2;
    //double yini[4] = {aini, aini, -3./7.*aini*aini, -6./7.*aini*aini};
    
    //FIXME: Consider a R+M only universe. 1LPT is Mesezaros, but 2LPT is currently wrong!
    //double aini = 1e-5;
    //double value = 1.5 * c->Omega_cdm / (aini*aini*aini*aini);
    //double yini[4] = {0, 0, -3./7.*aini*aini * value, -6./7.*aini*aini * value};
    //yini[0] = 1.5 * c->Omega_cdm / (aini*aini*aini) + Omega_r(c) / (aini*aini*aini*aini) + Omega_ncdmTimesHubbleEaSq(aini, c);
    //yini[1] = 1.5 * c->Omega_cdm / (aini*aini*aini);
    
    // ASSUME RD
    //double aini = 4e-2;
    //double aH = 1e-5;
    //double log_iH = log(aini / aH);
    //double yini[4] = {log_iH, 1, 0, 0};
    //yini[2] = - 3 / 2 * Omega_cdm_a(aini, c) * ( log_iH*log_iH - 4 * log_iH + 6 );     // Should really be Omega_m, but assume matter is just cdm at this time (i.e. all neutrinos are rel).
    //yini[3] = - 3 / 2 * Omega_cdm_a(aini, c) * ( log_iH*log_iH - 2 * log_iH + 2 );

    // ASSUME MD, FS
    double aini = 4e-2;
    double f = Omega_ncdm(1, c) / Omega_m(1, c);    //dont think I want to include relativistic nus?
    double p = 1./4. * (5 - sqrt(25 - 24 * f));
    double yini[4];
    yini[0] = pow(aini, 1-p);
    yini[1] = (1 - p) * yini[0];
    yini[2] = - 3./7. * (1 - f) / (1 - (9*f - 2*p)/7) * yini[0]*yini[0];
    yini[3] = 2 * (1 - p) * yini[2];
    
    //int stat = 
    gsl_odeiv2_driver_apply(drive, &aini, a, yini);
    //if (stat != GSL_SUCCESS) {     //If succesful, stat will = GSL_SUCCESS and yinit will become the final values...
        //endrun(1,"gsl_odeiv in growth: %d. Result at %g is %g %g\n",stat, curtime, yinit[0], yinit[1]); //need to define endrun
    //    printf(stat);    //quick fix for now 
    //}
    
    gsl_odeiv2_driver_free(drive);
    /*Store derivative of D if needed.*/
    //if(dDda) {
    //    *dDda = yinit[1]/pow(a,3)/(hubble_function(a)/CP->Hubble);
    //}
    
    ode_soln soln;
    soln.y0 = yini[0];
    soln.y1 = yini[1];
    soln.y2 = yini[2];
    soln.y3 = yini[3];
    
    return soln;
}

void fastpm_growth_info_init(FastPMGrowthInfo * growth_info, double a, FastPMCosmology * c) {
    ode_soln soln = growth_ode_solve(a, c);
    ode_soln soln_a1 = growth_ode_solve(1, c);
    // FIXME: you could save a=1 soln at the start of code (perhaps to cosmo obj) and then never compute again.

    growth_info->a = a;
    growth_info->c = c;
    growth_info->D1 = soln.y0 / soln_a1.y0;
    growth_info->f1 = soln.y1 / soln.y0;    /* f = d log D / d log a. Note soln.y1 is d d1 / d log a */
    growth_info->D2 = soln.y2 / soln_a1.y2;
    growth_info->f2 = soln.y3 / soln.y2;
}

/* FIXME: Some of the below growth functions are still called in io.c, horizon.c and solver.c.
          This shouldn't be a big deal, but might want to change to growth object later. */
double growth(double a, FastPMCosmology * c) {
    //d1
    return growth_ode_solve(a, c).y0;
}

double DgrowthDlna(double a, FastPMCosmology * c) {
    //F1
    return growth_ode_solve(a, c).y1;
}

double growth2(double a, FastPMCosmology * c) {
    //d2
    return growth_ode_solve(a, c).y2;
}

double Dgrowth2Dlna(double a, FastPMCosmology * c) {
    //F2
    return growth_ode_solve(a, c).y3;
}

double GrowthFactor(double a, FastPMCosmology * c) { // growth factor for LCDM
    return growth(a, c) / growth(1., c);           //[this is D(a)/D_today for LCDM]
}

double DLogGrowthFactor(double a, FastPMCosmology * c) {
    /* dlnD1/dlna */
    double d1 = growth(a, c);
    double F1 = DgrowthDlna(a, c);
    
    return F1 / d1;
}

double GrowthFactor2(double a, FastPMCosmology * c) {
    /* Normalised D2. Is this correct???*/
    // double d0 = growth(1., c);  //0 for today
    return growth2(a, c) / growth2(1., c); //(d0*d0); // ??????????????????;
}

double DLogGrowthFactor2(double a, FastPMCosmology * c) {
    /* dlnD2/dlna */
    double d2 = growth2(a, c);
    double F2 = Dgrowth2Dlna(a, c);
    
    return F2 / d2;
}

double DGrowthFactorDa(double a, FastPMCosmology * c) {
    double d0 = growth(1., c);
    double F1 = DgrowthDlna(a, c);
    return F1 / a / d0;
}

double D2GrowthFactorDa2(FastPMGrowthInfo * growth_info) {
    /* d^2 D1 / da^2 */
    double a = growth_info->a;
    FastPMCosmology * c = growth_info->c;

    double E = HubbleEa(a, c);
    double dEda = DHubbleEaDa(a, c);
    double D1 = growth_info->D1;
    double f1 = growth_info->f1;
    
    double ans = 0.;
    ans -= (3. + a / E * dEda) * f1 * D1;
    ans += 1.5 * Omega_m(a, c) * D1;
    ans /= (a*a);
    return ans;
}

static double
comoving_distance_int(double a, void * params)
{
    FastPMCosmology * c = (FastPMCosmology * ) params;
    return 1. / (a * a * HubbleEa(a, c));
}

double ComovingDistance(double a, FastPMCosmology * c) {

    /* We tested using ln_a doesn't seem to improve accuracy */
    int WORKSIZE = 100000;

    double result, abserr;
    gsl_integration_workspace *workspace;
    gsl_function F;

    workspace = gsl_integration_workspace_alloc(WORKSIZE);

    F.function = &comoving_distance_int;
    F.params = (void*) c;

    gsl_integration_qag(&F, a, 1., 0, 1.0e-8, WORKSIZE, GSL_INTEG_GAUSS41,
            workspace, &result, &abserr); 
            //lowered tol by /10 to avoid error from round off (maybe I need to make my paras more accurate)

    gsl_integration_workspace_free(workspace);

    return result;
}


#ifdef TEST_COSMOLOGY
int main() {
    /* the old COLA growthDtemp is 6 * pow(1 - c.OmegaM, 1.5) times growth */
    double a;
    FastPMCosmology c[1] = {{
        .OmegaM = 0.3,
        .OmegaLambda = 0.7
    }};

    printf("OmegaM D dD/da d2D/da2 D2 E dE/dA d2E/da2 \n");
    for(c->OmegaM = 0.1; c->OmegaM < 0.6; c->OmegaM += 0.1) {
        double f = 6 * pow(1 - c->OmegaM, 1.5);
        c->OmegaLambda = 1 - c->OmegaM;
        double a = 0.8;
        printf("%g %g %g %g %g %g %g %g %g\n",
            c->OmegaM, 
            ComovingDistance(a, c),
            GrowthFactor(a, c),
            DGrowthFactorDa(a, c),
            D2GrowthFactorDa2(a, c),

            GrowthFactor2(a, c),
            HubbleEa(a, c),
            DHubbleEaDa(a, c),
            D2HubbleEaDa2(a, c)
            );
    }
}


#endif
