/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "GU_PackedGolaemEntity.h"

#include "glmLog.h"

HDK_INCLUDES_START

#include <GEO/GEO_PolyCounts.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

HDK_INCLUDES_END

namespace glm
{
    GA_PrimitiveTypeId GU_PackedGolaemEntity::_typeId(-1);

    //-----------------------------------------------------------------------------
    const glm::GlmString& getEntityIdAttrName()
    {
        static glm::GlmString entityIdAttrName = "glmEntityId";
        return entityIdAttrName;
    }

    //-----------------------------------------------------------------------------
    class GU_PackedGolaemFactory : public GU_PackedFactory
    {
    public:
        GU_PackedGolaemFactory()
            : GU_PackedFactory("glmPackedEntity", "Golaem Entity")
        {
        }

        GU_PackedImpl* create() const override
        {
            return new GU_PackedGolaemEntity();
        }
    };

    static GU_PackedGolaemFactory* theGolaemFactory = NULL;

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity::GU_PackedGolaemEntity()
        : _rootPos()
        , _halfExtents()
        , _entityId(-1)
        , _updateGeo(false)
        , _pointStartOffset(0)
        , _primOffset(0)
        , _displayMode(GolaemDisplayMode::END)
    {
    }

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity::GU_PackedGolaemEntity(const GU_PackedGolaemEntity& src)
        : _rootPos(src._rootPos)
        , _halfExtents(src._halfExtents)
        , _entityId(src._entityId)
        , _updateGeo(false)
        , _pointStartOffset(src._pointStartOffset)
        , _primOffset(src._primOffset)
        , _displayMode(src._displayMode)
    {
        _detail = src._detail.duplicateGeometry();
    }

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity::~GU_PackedGolaemEntity()
    {
        clearGeo();
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::clearGeo()
    {
        _detail = GU_DetailHandle();
    }

    //-----------------------------------------------------------------------------
    GU_PackedGolaemEntity* GU_PackedGolaemEntity::build(GU_Detail* gdp)
    {
        GU_PrimPacked* packedPrim = GU_PrimPacked::build(*gdp, theGolaemFactory->typeDef().getId());
        GU_PackedGolaemEntity* packedEntity = static_cast<GU_PackedGolaemEntity*>(packedPrim->implementation());
        return packedEntity;
    }

    //-----------------------------------------------------------------------------
    const GA_PrimitiveTypeId& GU_PackedGolaemEntity::getTypeId()
    {
        return _typeId;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::install(GA_PrimitiveFactory* factory)
    {
        GLM_DEBUG_ASSERT(theGolaemFactory == NULL);
        if (theGolaemFactory != NULL)
            return;

        theGolaemFactory = new GU_PackedGolaemFactory();
        GU_PrimPacked::registerPacked(factory, theGolaemFactory);
        if (theGolaemFactory->isRegistered())
        {
            _typeId = theGolaemFactory->typeDef().getId();
            //GT_GEOPackedSphere::registerPrimitive(_typeId);
        }
        else
        {
            GLM_CROWD_TRACE_ERROR("Unable to register packed Golaem factory from " << UT_DSO::getRunningFile());
        }
    }

    //-----------------------------------------------------------------------------
    GU_PackedFactory* GU_PackedGolaemEntity::getFactory() const
    {
        return theGolaemFactory;
    }

    //-----------------------------------------------------------------------------
    GU_PackedImpl* GU_PackedGolaemEntity::copy() const
    {
        return new GU_PackedGolaemEntity(*this);
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::isValid() const
    {
        return _detail.isValid();
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::clearData()
    {
        // This method is called when primitives are "stashed" during the cooking
        // process.  However, primitives are typically immediately "unstashed" or
        // they are deleted if the primitives aren't recreated after the fact.
        // We can just leave our data.
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::load(GU_PrimPacked* prim, const UT_Options& options, const GA_LoadMap& map)
    {
        GLM_UNREFERENCED(prim);
        GLM_UNREFERENCED(options);
        GLM_UNREFERENCED(map);
        return false;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::update(GU_PrimPacked* prim, const UT_Options& options)
    {
        GLM_UNREFERENCED(prim);
        GLM_UNREFERENCED(options);
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::save(UT_Options& options, const GA_SaveMap& map) const
    {
        GLM_UNREFERENCED(options);
        GLM_UNREFERENCED(map);
        return false;
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::getBounds(UT_BoundingBox& box) const
    {
        GLM_UNREFERENCED(box);
        return false;
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::getRenderingBounds(UT_BoundingBox& box) const
    {
        GLM_UNREFERENCED(box);
        return false;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::getVelocityRange(UT_Vector3& min, UT_Vector3& max) const
    {
        GLM_UNREFERENCED(min);
        GLM_UNREFERENCED(max);
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::getWidthRange(fpreal& wmin, fpreal& wmax) const
    {
        wmin = wmax = 0; // Width is only important for curves/points.
    }

    //-----------------------------------------------------------------------------
    bool GU_PackedGolaemEntity::unpack(GU_Detail& destgdp, const UT_Matrix4D* transform) const
    {
        // This may allocate geometry for the primitive
        GU_DetailHandleAutoReadLock rlock(getPackedDetail());
        if (!rlock.getGdp())
        {
            return false;
        }
        return unpackToDetail(destgdp, rlock.getGdp(), transform);
    }

    //-----------------------------------------------------------------------------
    GU_ConstDetailHandle GU_PackedGolaemEntity::getPackedDetail(GU_PackedContext* context) const
    {
        GLM_UNREFERENCED(context);
        if (!_detail.isValid())
        {
            _detail.allocateAndSet(new GU_Detail());
            GEO_PolyCounts polyCounts;
            UT_IntArray polygonpointnumbers;

            GU_Detail* detailPtr = _detail.gdpNC();

            switch (_displayMode)
            {
            case glm::GolaemDisplayMode::BOUNDING_BOX:
            {
                // cube = 6 faces
                for (size_t iFace = 0; iFace < 6; ++iFace)
                {
                    polyCounts.append(4);
                }

                // face 0
                polygonpointnumbers.append(0);
                polygonpointnumbers.append(1);
                polygonpointnumbers.append(2);
                polygonpointnumbers.append(3);

                // face 1
                polygonpointnumbers.append(1);
                polygonpointnumbers.append(2);
                polygonpointnumbers.append(6);
                polygonpointnumbers.append(5);

                // face 2
                polygonpointnumbers.append(2);
                polygonpointnumbers.append(3);
                polygonpointnumbers.append(7);
                polygonpointnumbers.append(6);

                // face 3
                polygonpointnumbers.append(3);
                polygonpointnumbers.append(0);
                polygonpointnumbers.append(4);
                polygonpointnumbers.append(7);

                // face 4
                polygonpointnumbers.append(0);
                polygonpointnumbers.append(1);
                polygonpointnumbers.append(5);
                polygonpointnumbers.append(4);

                // face 5
                polygonpointnumbers.append(4);
                polygonpointnumbers.append(5);
                polygonpointnumbers.append(6);
                polygonpointnumbers.append(7);

                _pointStartOffset = detailPtr->appendPointBlock(8);
                _primOffset = GEO_PrimPoly::buildBlock(detailPtr, _pointStartOffset, 8, polyCounts, polygonpointnumbers.array(), false);
            }
            break;
            case glm::GolaemDisplayMode::SKELETON:
            {
            }
            break;
            case glm::GolaemDisplayMode::SKINMESH:
            {
            }
            break;
            default:
                break;
            }

            GA_Attribute* entityIdAttr = detailPtr->addTuple(GA_STORE_INT64, GA_ATTRIB_DETAIL, getEntityIdAttrName().c_str(), 1);
            GA_RWHandleID entityIdAttrHandle(entityIdAttr);
            entityIdAttrHandle.set(_primOffset, _entityId);
        }
        if (_updateGeo)
        {
            GU_Detail* detailPtr = _detail.gdpNC();

            switch (_displayMode)
            {
            case glm::GolaemDisplayMode::BOUNDING_BOX:
            {
                detailPtr->setPos3(_pointStartOffset,
                                   UT_Vector3(
                                       _rootPos[0] - _halfExtents[0],
                                       _rootPos[1] - _halfExtents[1],
                                       _rootPos[2] + _halfExtents[2]));

                detailPtr->setPos3(_pointStartOffset + 1,
                                   UT_Vector3(
                                       _rootPos[0] + _halfExtents[0],
                                       _rootPos[1] - _halfExtents[1],
                                       _rootPos[2] + _halfExtents[2]));

                detailPtr->setPos3(_pointStartOffset + 2,
                                   UT_Vector3(
                                       _rootPos[0] + _halfExtents[0],
                                       _rootPos[1] - _halfExtents[1],
                                       _rootPos[2] - _halfExtents[2]));

                detailPtr->setPos3(_pointStartOffset + 3,
                                   UT_Vector3(
                                       _rootPos[0] - _halfExtents[0],
                                       _rootPos[1] - _halfExtents[1],
                                       _rootPos[2] - _halfExtents[2]));

                detailPtr->setPos3(_pointStartOffset + 4,
                                   UT_Vector3(
                                       _rootPos[0] - _halfExtents[0],
                                       _rootPos[1] + _halfExtents[1],
                                       _rootPos[2] + _halfExtents[2]));

                detailPtr->setPos3(_pointStartOffset + 5,
                                   UT_Vector3(
                                       _rootPos[0] + _halfExtents[0],
                                       _rootPos[1] + _halfExtents[1],
                                       _rootPos[2] + _halfExtents[2]));

                detailPtr->setPos3(_pointStartOffset + 6,
                                   UT_Vector3(
                                       _rootPos[0] + _halfExtents[0],
                                       _rootPos[1] + _halfExtents[1],
                                       _rootPos[2] - _halfExtents[2]));

                detailPtr->setPos3(_pointStartOffset + 7,
                                   UT_Vector3(
                                       _rootPos[0] - _halfExtents[0],
                                       _rootPos[1] + _halfExtents[1],
                                       _rootPos[2] - _halfExtents[2]));
            }
            break;
            case glm::GolaemDisplayMode::SKELETON:
            {
            }
            break;
            case glm::GolaemDisplayMode::SKINMESH:
            {
            }
            break;
            default:
                break;
            }

            _updateGeo = false;
        }
        return _detail;
    }

    //-----------------------------------------------------------------------------
    int64 GU_PackedGolaemEntity::getMemoryUsage(bool inclusive) const
    {
        int64 mem = inclusive ? sizeof(*this) : 0;
        mem += _detail.getMemoryUsage(false);
        return mem;
    }

    //-----------------------------------------------------------------------------
    void GU_PackedGolaemEntity::countMemory(UT_MemoryCounter& counter, bool inclusive) const
    {
        if (counter.mustCountUnshared())
        {
            size_t mem = getMemoryUsage(inclusive);
            //UT_MEMORY_DEBUG_LOG("GU_PackedSphere", int64(mem));
            counter.countUnshared(mem);
        }
    }

} // namespace glm