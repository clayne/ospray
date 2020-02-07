// Copyright 2009-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/Managed.h"

namespace ospray {

//! Base class for Light objects
struct OSPRAY_SDK_INTERFACE Light : public ManagedObject
{
  Light();
  virtual ~Light() override = default;

  static Light *createInstance(const char *type);
  virtual void commit() override;
  virtual std::string toString() const override;

  //! get IE of a second light associated with the light type(if available)
  virtual utility::Optional<void *> getSecondIE();

  vec3f radiance{1.0f, 1.0f, 1.0f};
};

OSPTYPEFOR_SPECIALIZATION(Light *, OSP_LIGHT);

#define OSP_REGISTER_LIGHT(InternalClass, external_name)                       \
  OSP_REGISTER_OBJECT(::ospray::Light, light, InternalClass, external_name)

} // namespace ospray
