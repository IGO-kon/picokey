#include <stdint.h>

#include "btstack.h"

void picokey_sm_set_authentication_requirements(uint8_t auth_req)
{
    // Keep bonding/secure-connection enabled even if the demo relaxes auth later.
    uint8_t required = (uint8_t)(auth_req | SM_AUTHREQ_BONDING | SM_AUTHREQ_SECURE_CONNECTION);
    sm_set_authentication_requirements(required);
}
