


//
 // CopyThreadContext() does an intelligent copy from pSrc to pDst,
 // respecting the ContextFlags of both contexts.
 //
 void CORDbgCopyThreadContext(DT_CONTEXT* pDst, const DT_CONTEXT* pSrc)
 {
     _ASSERTE(!"NYI");
 }
 
 void CORDbgSetDebuggerREGDISPLAYFromContext(DebuggerREGDISPLAY* pDRD,
                                             DT_CONTEXT* pContext)
 {
     _ASSERTE(!"NYI");
 }
 
 #if defined(ALLOW_VMPTR_ACCESS) || !defined(RIGHT_SIDE_COMPILE)
 void SetDebuggerREGDISPLAYFromREGDISPLAY(DebuggerREGDISPLAY* pDRD, REGDISPLAY* pRD)
 {
     _ASSERTE(!"NYI");
 }
 #endif // ALLOW_VMPTR_ACCESS || !RIGHT_SIDE_COMPILE