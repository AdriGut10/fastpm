FASTPM_BEGIN_DECLS

typedef struct VPMInit {
    double a_start;
    int pm_nc_factor;
} VPMInit;


enum FastPMExtensionPoint {
    FASTPM_EXT_AFTER_FORCE = 0,
    FASTPM_EXT_AFTER_KICK = 1,
    FASTPM_EXT_AFTER_DRIFT = 2,
    FASTPM_EXT_MAX = 3,
};
typedef struct FastPMExtension {
    void * function;
    void * userdata;
    struct FastPMExtension * next;
} FastPMExtension;

typedef struct {
    /* input parameters */
    size_t nc;
    double boxsize;
    double omega_m;
    double alloc_factor;
    VPMInit * vpminit;
    int USE_COLA;
    int USE_NONSTDDA;
    int USE_LINEAR_THEORY;
    double nLPT;
    double K_LINEAR;

    /* Extensions */
    FastPMExtension * exts[3];

    /* internal variables */
    MPI_Comm comm;
    int ThisTask;
    int NTask;

    PMStore * p;
    PM * pm_2lpt;
    VPM * vpm_list;

    /* Pointer to the current PM object */
    PM * pm;
} FastPM;

typedef int (* fastpm_ext_after_force) (FastPM * fastpm, FastPMFloat * deltak, double a_x, void * userdata);
typedef int (* fastpm_ext_after_kick) (FastPM * fastpm, void * userdata);
typedef int (* fastpm_ext_after_drift) (FastPM * fastpm, void * userdata);

void fastpm_init(FastPM * fastpm, 
    int NprocY,  /* Use 0 for auto */
    int UseFFTW, /* Use 0 for PFFT 1 for FFTW */
    MPI_Comm comm);

void 
fastpm_add_extension(FastPM * fastpm, 
    enum FastPMExtensionPoint where,
    void * function, void * userdata);

void 
fastpm_destroy(FastPM * fastpm);

void 
fastpm_solve_2lpt(FastPM * fastpm, FastPMFloat * delta_k_ic);

void
fastpm_evolve(FastPM * fastpm, double * time_step, int nstep);

typedef int 
(*fastpm_interp_action) (FastPM * fastpm, PMStore * pout, double aout, void * userdata);

void 
fastpm_interp(FastPM * fastpm, double * aout, int nout, 
            fastpm_interp_action action, void * userdata);

FASTPM_END_DECLS
