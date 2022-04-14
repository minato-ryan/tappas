/**
* Copyright (c) 2021-2022 Hailo Technologies Ltd. All rights reserved.
* Distributed under the LGPL license (https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt)
**/
#ifndef _HAILO_FACIAL_LANDMARKS_POST_HPP_
#define _HAILO_FACIAL_LANDMARKS_POST_HPP_
#include "hailo_objects.hpp"
#include "hailo_common.hpp"

__BEGIN_DECLS
void filter(HailoROIPtr roi);
// Post-process function used to add landmarks on given detection.
// Used for Face Detection + Face Landmarks app.
void facial_landmarks_merged(HailoROIPtr roi);
__END_DECLS

#endif