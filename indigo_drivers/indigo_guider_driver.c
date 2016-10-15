//  Copyright (c) 2016 CloudMakers, s. r. o.
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//  1. Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above
//  copyright notice, this list of conditions and the following
//  disclaimer in the documentation and/or other materials provided
//  with the distribution.
//
//  3. The name of the author may not be used to endorse or promote
//  products derived from this software without specific prior
//  written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE AUTHOR 'AS IS' AND ANY EXPRESS
//  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
//  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
//  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//  version history
//  2.0 Build 0 - PoC by Peter Polakovic <peter.polakovic@cloudmakers.eu>

/** INDIGO Guider Driver base
 \file indigo_guider_driver.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "indigo_guider_driver.h"

indigo_result indigo_guider_device_attach(indigo_device *device, char *name, indigo_version version) {
  assert(device != NULL);
  assert(device != NULL);
  if (GUIDER_DEVICE_CONTEXT == NULL) {
    device->device_context = malloc(sizeof(indigo_guider_device_context));
    memset(device->device_context, 0, sizeof(indigo_guider_device_context));
  }
  if (GUIDER_DEVICE_CONTEXT != NULL) {
    if (indigo_device_attach(device, version, INDIGO_INTERFACE_GUIDER) == INDIGO_OK) {
      // -------------------------------------------------------------------------------- GUIDER_GUIDE_DEC
      GUIDER_GUIDE_DEC_PROPERTY = indigo_init_switch_property(NULL, name, "GUIDER_GUIDE_DEC", GUIDER_MAIN_GROUP, "DEC guiding", INDIGO_IDLE_STATE, INDIGO_RW_PERM, INDIGO_ONE_OF_MANY_RULE, 2);
      if (GUIDER_GUIDE_DEC_PROPERTY == NULL)
        return INDIGO_FAILED;
      GUIDER_GUIDE_DEC_PROPERTY->hidden = true;
      indigo_init_switch_item(GUIDER_GUIDE_NORTH_ITEM, "GUIDER_GUIDE_NORTH", "Guide north", false);
      indigo_init_switch_item(GUIDER_GUIDE_SOUTH_ITEM, "GUIDER_GUIDE_SOUTH", "Guide south", false);
      // -------------------------------------------------------------------------------- GUIDER_GUIDE_RA
      GUIDER_GUIDE_RA_PROPERTY = indigo_init_switch_property(NULL, name, "GUIDER_GUIDE_RA", GUIDER_MAIN_GROUP, "RA guiding", INDIGO_IDLE_STATE, INDIGO_RW_PERM, INDIGO_ONE_OF_MANY_RULE, 2);
      if (GUIDER_GUIDE_RA_PROPERTY == NULL)
        return INDIGO_FAILED;
      GUIDER_GUIDE_RA_PROPERTY->hidden = true;
      indigo_init_switch_item(GUIDER_GUIDE_WEST_ITEM, "GUIDER_GUIDE_WEST", "Guide west", false);
      indigo_init_switch_item(GUIDER_GUIDE_EAST_ITEM, "GUIDER_GUIDE_EAST", "Guide east", false);
      // --------------------------------------------------------------------------------
      return INDIGO_OK;
    }
  }
  return INDIGO_FAILED;
}

indigo_result indigo_guider_device_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property) {
  assert(device != NULL);
  assert(device->device_context != NULL);
  indigo_result result = INDIGO_OK;
  if ((result = indigo_device_enumerate_properties(device, client, property)) == INDIGO_OK) {
    if (CONNECTION_CONNECTED_ITEM->sw.value) {
      if (indigo_property_match(GUIDER_GUIDE_DEC_PROPERTY, property))
        indigo_define_property(device, GUIDER_GUIDE_DEC_PROPERTY, NULL);
      if (indigo_property_match(GUIDER_GUIDE_RA_PROPERTY, property))
        indigo_define_property(device, GUIDER_GUIDE_RA_PROPERTY, NULL);
    }
  }
  return result;
}

indigo_result indigo_guider_device_change_property(indigo_device *device, indigo_client *client, indigo_property *property) {
  assert(device != NULL);
  assert(device->device_context != NULL);
  assert(property != NULL);
  if (indigo_property_match(CONNECTION_PROPERTY, property)) {
    // -------------------------------------------------------------------------------- CONNECTION
    if (CONNECTION_CONNECTED_ITEM->sw.value) {
      indigo_define_property(device, GUIDER_GUIDE_DEC_PROPERTY, NULL);
      indigo_define_property(device, GUIDER_GUIDE_RA_PROPERTY, NULL);
    } else {
      indigo_delete_property(device, GUIDER_GUIDE_DEC_PROPERTY, NULL);
      indigo_delete_property(device, GUIDER_GUIDE_RA_PROPERTY, NULL);
    }
    // --------------------------------------------------------------------------------
  }
  return indigo_device_change_property(device, client, property);
}

indigo_result indigo_guider_device_detach(indigo_device *device) {
  assert(device != NULL);
  if (CONNECTION_CONNECTED_ITEM->sw.value) {
    indigo_delete_property(device, GUIDER_GUIDE_DEC_PROPERTY, NULL);
    indigo_delete_property(device, GUIDER_GUIDE_RA_PROPERTY, NULL);
  }
  free(GUIDER_GUIDE_DEC_PROPERTY);
  free(GUIDER_GUIDE_RA_PROPERTY);
  return indigo_device_detach(device);
}
