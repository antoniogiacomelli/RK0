/******************************************************************************
 *
 *     [[RK0] | [VERSION: 0.4.0]]
 *
 ******************************************************************************
 ******************************************************************************
 * 	In this header:
 * 					o Private API: Error handling and Checks
 *
 *****************************************************************************/

#ifndef RK_ERR_H
#define RK_ERR_H
#ifdef __cplusplus
extern "C" {
#endif
extern volatile RK_FAULT faultID;
VOID ITM_SendValue(UINT);
VOID kErrHandler(RK_FAULT);
#ifdef __cplusplus
}
#endif
#endif /* RK_ERR_H*/
