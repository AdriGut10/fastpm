void 
fastpm_apply_smoothing_transfer(PM * pm, FastPMFloat * from, FastPMFloat * to, double sml);

void 
fastpm_apply_lowpass_transfer(PM * pm, FastPMFloat * from, FastPMFloat * to, double kth);

void 
fastpm_apply_decic_transfer(PM * pm, FastPMFloat * from, FastPMFloat * to);

void 
fastpm_apply_diff_transfer(PM * pm, FastPMFloat * from, FastPMFloat * to, int dir) ;

void fastpm_apply_za_hmc_force_transfer(PM * pm, FastPMFloat * from, FastPMFloat * to, int dir);
