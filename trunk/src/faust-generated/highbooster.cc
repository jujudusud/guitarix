// generated from file '../src/faust/highbooster.dsp' by dsp2cc:
// Code generated with Faust 0.9.46 (http://faust.grame.fr)


namespace highbooster {

class Dsp: public PluginDef {
private:
	int fSamplingFreq;
	double 	fConst0;
	double 	fConst1;
	double 	fConst2;
	double 	fVec0[2];
	double 	fConst3;
	double 	fConst4;
	double 	fRec0[2];
	FAUSTFLOAT 	fslider0;
	void clear_state_f();
	void init(unsigned int samplingFreq);
	void compute(int count, float *input0, float *output0);
	int register_par(const ParamReg& reg);

	static void clear_state_f_static(PluginDef*);
	static void init_static(unsigned int samplingFreq, PluginDef*);
	static void compute_static(int count, float *input0, float *output0, PluginDef*);
	static int register_params_static(const ParamReg& reg);
	static void del_instance(PluginDef *p);

public:
	Dsp();
	~Dsp();
};



Dsp::Dsp()
	: PluginDef() {
	version = PLUGINDEF_VERSION;
	flags = 0;
	id = "highbooster";
	name = N_("Treble boost");
	groups = 0;
	description = ""; // description (tooltip)
	category = N_("Tone control");       // category
	shortname = "";     // shortname
	mono_audio = compute_static;
	stereo_audio = 0;
	set_samplerate = init_static;
	activate_plugin = 0;
	register_params = register_params_static;
	load_ui = 0;
	clear_state = clear_state_f_static;
	delete_instance = del_instance;
}

Dsp::~Dsp() {
}

inline void Dsp::clear_state_f()
{
	for (int i=0; i<2; i++) fVec0[i] = 0;
	for (int i=0; i<2; i++) fRec0[i] = 0;
}

void Dsp::clear_state_f_static(PluginDef *p)
{
	static_cast<Dsp*>(p)->clear_state_f();
}

inline void Dsp::init(unsigned int samplingFreq)
{
	fSamplingFreq = samplingFreq;
	fConst0 = (1.0 / tan((4712.38898038469 / min(192000, max(1, fSamplingFreq)))));
	fConst1 = (1 + fConst0);
	fConst2 = (0 - ((1 - fConst0) / fConst1));
	fConst3 = (0 - fConst0);
	fConst4 = (1.0 / fConst1);
	clear_state_f();
}

void Dsp::init_static(unsigned int samplingFreq, PluginDef *p)
{
	static_cast<Dsp*>(p)->init(samplingFreq);
}

inline void Dsp::compute(int count, float *input0, float *output0)
{
	double 	fSlow0 = (pow(10,(0.05 * fslider0)) - 1);
	for (int i=0; i<count; i++) {
		double fTemp0 = (double)input0[i];
		fVec0[0] = fTemp0;
		fRec0[0] = ((fConst4 * ((fConst3 * fVec0[1]) + (fConst0 * fVec0[0]))) + (fConst2 * fRec0[1]));
		output0[i] = (FAUSTFLOAT)(fVec0[0] + (fSlow0 * fRec0[0]));
		// post processing
		fRec0[1] = fRec0[0];
		fVec0[1] = fVec0[0];
	}
}

void Dsp::compute_static(int count, float *input0, float *output0, PluginDef *p)
{
	static_cast<Dsp*>(p)->compute(count, input0, output0);
}

int Dsp::register_par(const ParamReg& reg)
{
	reg.registerVar("highbooster.Level","","S","",&fslider0, 0.0, 0.5, 2e+01, 0.5);
	return 0;
}

int Dsp::register_params_static(const ParamReg& reg)
{
	return static_cast<Dsp*>(reg.plugin)->register_par(reg);
}

PluginDef *plugin() {
	return new Dsp();
}

void Dsp::del_instance(PluginDef *p)
{
	delete static_cast<Dsp*>(p);
}

} // end namespace highbooster