
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the PCSC_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// PCSC_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef PCSC_EXPORTS
#define PCSC_API __declspec(dllexport)
#else
#define PCSC_API __declspec(dllimport)
#endif

// This class is exported from the PCSC.dll
//class PCSC_API CPCSC {
//public:
//	CPCSC(void);
//	// TODO: add your methods here.
//};

//extern PCSC_API int nPCSC;

//PCSC_API int fnPCSC(void);

