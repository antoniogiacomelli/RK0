/******************************************************************************
 *
 *     [[RK0] | [VERSION: 0.4.0]]
 *
 ******************************************************************************
 ******************************************************************************
 *  In this header:
 *                  o Kernel Version record definition
 *                  xx.xx.xx
 *                  major minor patch
 *
 *****************************************************************************/
#ifndef RK_VERSION_H
#define RK_VERSION_H

/*** Minimal valid version **/
/** This is to manage API retrocompatibilities */
#define RK_CONF_MINIMAL_VER        0U

extern struct kversion const KVERSION;

#if (RK_CONF_MINIMAL_VER==0U) /* there is no retrocompatible version */
                            /* the valid is the current            */
#define RK_VALID_VERSION (unsigned)((KVERSION.major << 16 | KVERSION.minor << 8 \
            | KVERSION.patch << 0))

#else

#define RK_VALID_VERSION RK_CONF_MINIMAL_VER

#endif

struct kversion
{
    unsigned char major;
    unsigned char minor;
    unsigned char patch;
};

unsigned int kGetVersion(void);
unsigned int kIsValidVersion(void);
#endif /* KVERSION_H */
