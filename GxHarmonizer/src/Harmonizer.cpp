#include <stdlib.h>
#include <cmath>
#include "PitchShifterClasses.h"
#include "PitchDetectionClass.h"
#include "HarmonizerLib.h"
#include "GainClass.h"

/**********************************************************************************************************************************************************/

#define PLUGIN_URI "http://moddevices.com/plugins/mod-devel/GxHarmonizer"
#define FIDELITY0 4,2,1,1
#define FIDELITY1 8,4,2,1
#define FIDELITY2 12,6,3,2
#define FIDELITYPD 8,4,2,1
enum {IN, OUT_1, OUT_2, TONE, SCALE, INTERVAL, MODE, LOWNOTE, GAIN_1, GAIN_2, FIDELITY, PLUGIN_PORT_COUNT};

/**********************************************************************************************************************************************************/

class Harmonizer
{
public:
    Harmonizer(uint32_t n_samples, int nBuffers, int nBuffers2, double samplerate, const std::string& wfile)
    {
        wisdomFile = wfile;
        Construct(n_samples, nBuffers, nBuffers2, samplerate, wfile.c_str());
    }
    ~Harmonizer(){Destruct();}
    void Construct(uint32_t n_samples, int nBuffers, int nBuffers2, double samplerate, const char* wisdomFile)
    {
        this->nBuffers = nBuffers;
        this->nBuffers2 = nBuffers2;
        SampleRate = samplerate;

        obja = new PSAnalysis(n_samples, nBuffers, wisdomFile);
        objs = new PSSinthesis(obja, wisdomFile);
        objpd = new PitchDetection(n_samples, nBuffers2, samplerate, wisdomFile);
        objg1 = new GainClass(n_samples);
        objg2 = new GainClass(n_samples);

        cont = 0;
        s = 0;
    }
    void Destruct()
    {
        delete obja;
        delete objs;
        delete objpd;
        delete objg1;
        delete objg2;
    }
    void Realloc(uint32_t n_samples, int nBuffers, int nBuffers2)
    {
        Destruct();
        Construct(n_samples, nBuffers, nBuffers2, SampleRate, wisdomFile.c_str());
    }

    void SetFidelity(int fidelity, uint32_t n_samples)
    {
        int bufsize;

        switch (fidelity)
        {
        case 0: 
            bufsize = nBuffersSW(n_samples,FIDELITY0);
            break;
        case 1:
            bufsize = nBuffersSW(n_samples,FIDELITY1);
            break;
        case 2:
            bufsize = nBuffersSW(n_samples,FIDELITY2);
            break;
        default:
            return;
        }

        if (nBuffers != bufsize || obja->hopa != (int)n_samples)
            Realloc(n_samples, bufsize, nBuffersSW(n_samples,FIDELITYPD));
    }

    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void activate(LV2_Handle instance);
    static void deactivate(LV2_Handle instance);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
    static void run(LV2_Handle instance, uint32_t n_samples);
    static void cleanup(LV2_Handle instance);
    static const void* extension_data(const char* uri);
    float *ports[PLUGIN_PORT_COUNT];
    
    PSAnalysis *obja;
    PSSinthesis *objs;
    PitchDetection *objpd;
    GainClass *objg1;
    GainClass *objg2;

    int nBuffers;
    int nBuffers2;
    int cont;
    double SampleRate;
    double s;
    std::string wisdomFile;
};

/**********************************************************************************************************************************************************/

static const LV2_Descriptor Descriptor = {
    PLUGIN_URI,
    Harmonizer::instantiate,
    Harmonizer::connect_port,
    Harmonizer::activate,
    Harmonizer::run,
    Harmonizer::deactivate,
    Harmonizer::cleanup,
    Harmonizer::extension_data
};

/**********************************************************************************************************************************************************/

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;
}

/**********************************************************************************************************************************************************/

LV2_Handle Harmonizer::instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features)
{
    std::string wisdomFile = bundle_path;
    wisdomFile += "/harmonizer.wisdom";
    const uint32_t n_samples = 256; // Fix the buffersize for Guitarix //GetBufferSize(features);
    Harmonizer *plugin = new Harmonizer(n_samples, nBuffersSW(n_samples,FIDELITY1), nBuffersSW(n_samples,FIDELITYPD), samplerate, wisdomFile);
    return (LV2_Handle)plugin;
}

/**********************************************************************************************************************************************************/

void Harmonizer::activate(LV2_Handle instance){}

/**********************************************************************************************************************************************************/

void Harmonizer::deactivate(LV2_Handle instance){}

/**********************************************************************************************************************************************************/

void Harmonizer::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Harmonizer *plugin;
    plugin = (Harmonizer *) instance;
    plugin->ports[port] = (float*) data;
}

/**********************************************************************************************************************************************************/

void Harmonizer::run(LV2_Handle instance, uint32_t n_samples)
{
    Harmonizer *plugin;
    plugin = (Harmonizer *) instance;

    float  *in      = plugin->ports[IN];
    float  *out_1   = plugin->ports[OUT_1];
    float  *out_2   = plugin->ports[OUT_2];
    int    Tone     = (int)(*(plugin->ports[TONE])+0.5f);
    int    Scale    = (int)(*(plugin->ports[SCALE])+0.5f);
    int    Interval = (int)(*(plugin->ports[INTERVAL])+0.5f);
    int    Mode     = (int)(*(plugin->ports[MODE])+0.5f);
    int    LowNote  = (int)(*(plugin->ports[LOWNOTE])+0.5f);
    double gain_1   = (double)(*(plugin->ports[GAIN_1]));
    double gain_2   = (double)(*(plugin->ports[GAIN_2]));
    int    fidelity = (int)(*(plugin->ports[FIDELITY])+0.5f);
    
    plugin->SetFidelity(fidelity, n_samples);

	if (InputAbsSum(in, n_samples) == 0)
	{
        memset(out_1,0,sizeof(float)*n_samples);
        memset(out_2,0,sizeof(float)*n_samples);
        return;
	}

    (plugin->objg1)->SetGaindB(gain_1);
    (plugin->objg2)->SetGaindB(gain_2);
    
    (plugin->obja)->PreAnalysis(plugin->nBuffers, in);
    (plugin->objs)->PreSinthesis();
    (plugin->objpd)->PreProcessing(plugin->nBuffers2, in);
	
	if (plugin->cont < plugin->nBuffers-1)
		plugin->cont = plugin->cont + 1;
	else
	{
		(plugin->objpd)->FindNote();
        FindStep((plugin->objpd)->note, (plugin->objpd)->oitava, Tone, Scale, Interval, Mode, LowNote, &plugin->s);
        (plugin->obja)->Analysis();
        (plugin->objs)->Sinthesis(plugin->s);
        (plugin->objg1)->SimpleGain((plugin->obja)->frames, out_1);
        (plugin->objg2)->SimpleGain((plugin->objs)->yshift, out_2);
	}
}

/**********************************************************************************************************************************************************/

void Harmonizer::cleanup(LV2_Handle instance)
{
    delete ((Harmonizer *) instance);
}

/**********************************************************************************************************************************************************/

const void* Harmonizer::extension_data(const char* uri)
{
    return NULL;
}
