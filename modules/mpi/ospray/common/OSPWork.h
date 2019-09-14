// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

/*! \file OSPWork.h implements everything require to encode and
  serialize work items that represent api calls

  this code currently lives only in the mpi device, but shuld in
  theory also be applicable to other sorts of 'fabrics' for conveying
  such encoded work items
*/

#pragma once

#include <ospray/ospray.h>
#include "common/ObjectHandle.h"
#include "mpiCommon/MPICommon.h"

#include "ospcommon/networking/DataStreaming.h"
#include "ospcommon/utility/ArrayView.h"

#include "camera/Camera.h"
#include "common/Instance.h"
#include "common/World.h"
#include "geometry/Geometry.h"
#include "lights/Light.h"
#include "render/Renderer.h"
#include "fb/ImageOp.h"
#include "transferFunction/TransferFunction.h"
#include "volume/Volume.h"

#include <map>

namespace ospray {
  namespace mpi {
    namespace work {

      using namespace mpicommon;
      using namespace ospcommon::networking;

      /*! abstract interface for a work item. a work item can
        serialize itself, de-serialize itself, and return a tag that
        allows the unbuffering code form figuring out what kind of
        work this is */
      struct Work
      {
        virtual ~Work() = default;
        /*! type we use for representing tags */
        using tag_t = size_t;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        virtual void serialize(WriteStream &b) const = 0;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        virtual void deserialize(ReadStream &b) = 0;

        /*! what to do to execute this work item on a worker */
        virtual void run() {}

        /*! what to do to execute this work item on the master */
        virtual void runOnMaster() {}
      };

      using CreateWorkFct    = std::unique_ptr<Work> (*)();
      using WorkTypeRegistry = std::map<Work::tag_t, CreateWorkFct>;

      /*! create a work unit of given type */
      template <typename T>
      inline std::unique_ptr<Work> make_work_unit()
      {
        return make_unique<T>();
      }

      template <typename T>
      inline CreateWorkFct createMakeWorkFct()
      {
        return make_work_unit<T>;
      }

      template <typename WORK_T>
      inline void registerWorkUnit(WorkTypeRegistry &registry)
      {
        static_assert(std::is_base_of<Work, WORK_T>::value,
                      "WORK_T must be a child class of ospray::work::Work!");

        registry[typeIdOf<WORK_T>()] = createMakeWorkFct<WORK_T>();
      }

      void registerOSPWorkItems(WorkTypeRegistry &registry);

      /*! this should go into implementation section ... */
      struct SetLoadBalancer : public Work
      {
        SetLoadBalancer() = default;
        SetLoadBalancer(ObjectHandle _handle,
                        bool _useDynamicLoadBalancer,
                        int _numTilesPreAllocated = 4);

        void run() override;
        void runOnMaster() override;

        void serialize(WriteStream &b) const override;
        void deserialize(ReadStream &b) override;

       private:
        int useDynamicLoadBalancer{false};
        int numTilesPreAllocated{4};
        int64 handleID{-1};
      };

      template <typename T>
      struct NewObjectT : public Work
      {
        NewObjectT() = default;
        NewObjectT(const char *type, ObjectHandle handle)
            : type(type), handle(handle)
        {
        }

        void run() override
        {
          auto *obj = T::createInstance(type.c_str());
          handle.assign(obj);
        }

        void runOnMaster() override {}

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << type;
        }

        /*! de-serialize from a buffer that an object of this type ha
          serialized itself in */
        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> type;
        }

        std::string type;
        ObjectHandle handle;
      };

      // NewObjectT explicit instantiations ///////////////////////////////////

      using NewWorld            = NewObjectT<World>;
      using NewGroup            = NewObjectT<Group>;
      using NewImageOp          = NewObjectT<ImageOp>;
      using NewRenderer         = NewObjectT<Renderer>;
      using NewCamera           = NewObjectT<Camera>;
      using NewVolume           = NewObjectT<Volume>;
      using NewGeometry         = NewObjectT<Geometry>;
      using NewTransferFunction = NewObjectT<TransferFunction>;
      using NewTexture          = NewObjectT<Texture>;

      // Specializations for objects
      template <>
      void NewRenderer::runOnMaster();

      template <>
      void NewVolume::runOnMaster();

      template <>
      void NewImageOp::runOnMaster();

      template <>
      void NewWorld::run();

      template <>
      void NewGroup::run();

      template<>
      void NewCamera::runOnMaster();

      struct NewMaterial : public Work
      {
        NewMaterial() = default;
        NewMaterial(const char *renderer_type,
                    const char *material_type,
                    ObjectHandle handle)
            : rendererType(renderer_type),
              materialType(material_type),
              handle(handle)
        {
        }

        void run() override;

        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << rendererType << materialType;
        }

        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> rendererType >> materialType;
        }

        std::string rendererType;
        std::string materialType;
        ObjectHandle handle;
      };

      struct NewInstance : public Work
      {
        NewInstance() = default;
        NewInstance(ObjectHandle handle, ObjectHandle group_handle)
            : handle(handle), groupHandle(group_handle)
        {
        }

        void run() override;

        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << (int64)groupHandle;
        }

        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> groupHandle.i64;
        }

        ObjectHandle handle;
        ObjectHandle groupHandle;
      };

      struct NewGeometricModel : public Work
      {
        NewGeometricModel() = default;
        NewGeometricModel(ObjectHandle handle, ObjectHandle geometry_handle)
            : handle(handle), geometryHandle(geometry_handle)
        {
        }

        void run() override;

        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << (int64)geometryHandle;
        }

        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> geometryHandle.i64;
        }

        ObjectHandle handle;
        ObjectHandle geometryHandle;
      };

      struct NewVolumetricModel : public Work
      {
        NewVolumetricModel() = default;
        NewVolumetricModel(ObjectHandle handle, ObjectHandle volume_handle)
            : handle(handle), volumeHandle(volume_handle)
        {
        }

        void run() override;

        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << (int64)volumeHandle;
        }

        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> volumeHandle.i64;
        }

        ObjectHandle handle;
        ObjectHandle volumeHandle;
      };

      struct NewLight : public Work
      {
        NewLight() = default;
        NewLight(const char *type, ObjectHandle handle)
            : type(type), handle(handle)
        {
        }

        void run() override;

        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << type;
        }

        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> type;
        }

        std::string type;
        ObjectHandle handle;
      };

      struct NewData : public Work
      {
        NewData() = default;
        NewData(ObjectHandle handle,
                size_t nItems,
                OSPDataType format,
                const void *initData,
                int flags);

        void run() override;

        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle handle;
        size_t nItems;
        OSPDataType format;

        std::vector<byte_t> copiedData;
        utility::ArrayView<byte_t>
            dataView;  //<-- may point to user data or
                       //    'copiedData' member, depending
                       //    on flags given on construction

        int32 flags;
      };

      struct CommitObject : public Work
      {
        CommitObject() = default;
        CommitObject(ObjectHandle handle);

        void run() override;
        // TODO: Which objects should the master commit?
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle handle;
      };

      struct ResetAccumulation : public Work
      {
        ResetAccumulation() = default;
        ResetAccumulation(OSPFrameBuffer fb);

        void run() override;
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle handle;
      };

      struct RenderFrameAsync : public Work
      {
        RenderFrameAsync() = default;
        RenderFrameAsync(OSPFrameBuffer fb,
                         OSPRenderer renderer,
                         OSPCamera camera,
                         OSPWorld world,
                         ObjectHandle futureHandle);

        void run() override;
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle fbHandle;
        ObjectHandle rendererHandle;
        ObjectHandle cameraHandle;
        ObjectHandle worldHandle;
        ObjectHandle futureHandle;
      };

      struct CreateFrameBuffer : public Work
      {
        CreateFrameBuffer() = default;
        CreateFrameBuffer(ObjectHandle handle,
                          vec2i dimensions,
                          OSPFrameBufferFormat format,
                          uint32 channels);

        void run() override;
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle handle;
        vec2i dimensions{-1};
        OSPFrameBufferFormat format;
        uint32 channels;
      };

      /*! this should go into implementation section ... */
      template <typename T>
      struct SetParam : public Work
      {
        SetParam() = default;

        SetParam(ObjectHandle handle, const char *name, const T &val)
            : handle(handle), name(name), val(val)
        {
          assert(handle != nullHandle);
        }

        inline void run() override
        {
          ManagedObject *obj = handle.lookup();
          assert(obj);
          obj->setParam(name, val);
        }

        void runOnMaster() override
        {
          if (!handle.defined())
            return;

          ManagedObject *obj = handle.lookup();
          if (dynamic_cast<Renderer *>(obj) || dynamic_cast<Volume *>(obj)
              || dynamic_cast<FrameBuffer *>(obj) || dynamic_cast<Camera *>(obj))
          {
            obj->setParam(name, val);
          }
        }

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << name << val;
        }

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> name >> val;
        }

        ObjectHandle handle;
        std::string name;
        T val;
      };

      // run for setString needs to know to pass the C string to
      // set the param so we need to provide a different run.
      template <>
      void SetParam<std::string>::run();

      template <>
      void SetParam<std::string>::runOnMaster();

      template <>
      struct SetParam<OSPObject> : public Work
      {
        SetParam() = default;

        SetParam(ObjectHandle handle, const char *name, OSPObject &obj)
            : handle(handle), name(name), val((ObjectHandle &)obj)
        {
          assert(handle != nullHandle);
        }

        void run() override
        {
          ManagedObject *obj = handle.lookup();
          assert(obj);
          ManagedObject *param = NULL;
          if (val != NULL_HANDLE) {
            param = val.lookup();
            assert(param);
          }
          obj->setParam(name, param);
        }

        void runOnMaster() override
        {
          if (!handle.defined() || !val.defined())
            return;

          ManagedObject *obj = handle.lookup();
          if (dynamic_cast<Renderer *>(obj) || dynamic_cast<Volume *>(obj)
            || dynamic_cast<FrameBuffer *>(obj) || dynamic_cast<Camera *>(obj))
          {
            obj->setParam(name, val.lookup());
          }
        }


        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override
        {
          b << (int64)handle << name << (int64)val;
        }

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override
        {
          b >> handle.i64 >> name >> val.i64;
        }

        ObjectHandle handle;
        std::string name;
        ObjectHandle val;
      };

      struct RemoveParam : public Work
      {
        RemoveParam() = default;
        RemoveParam(ObjectHandle handle, const char *name);

        void run() override;
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle handle;
        std::string name;
      };

      struct CommandRelease : public Work
      {
        CommandRelease() = default;
        CommandRelease(ObjectHandle handle);

        void run() override;
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle handle;
      };

      struct LoadModule : public Work
      {
        LoadModule() = default;
        LoadModule(const std::string &name);

        void run() override;
        // We do need to load modules on master in the case of scripted modules
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        std::string name;
        int errorCode{0};
      };

      struct CommandFinalize : public Work
      {
        CommandFinalize() = default;

        void run() override;
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;
      };

      struct Pick : public Work
      {
        Pick() = default;
        Pick(OSPFrameBuffer fb,
             OSPRenderer renderer,
             OSPCamera camera,
             OSPWorld world,
             const vec2f &screenPos);

        void run() override;
        void runOnMaster() override;

        /*! serializes itself on the given serial buffer - will write
          all data into this buffer in a way that it can afterwards
          un-serialize itself 'on the other side'*/
        void serialize(WriteStream &b) const override;

        /*! de-serialize from a buffer that an object of this type has
          serialized itself in */
        void deserialize(ReadStream &b) override;

        ObjectHandle fbHandle;
        ObjectHandle rendererHandle;
        ObjectHandle cameraHandle;
        ObjectHandle worldHandle;
        vec2f screenPos;
        OSPPickResult pickResult;
      };

    }  // namespace work
  }    // namespace mpi
}  // namespace ospray