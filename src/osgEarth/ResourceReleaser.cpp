/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
 * Copyright 2008-2014 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include <osgEarth/ResourceReleaser>
#include <osgEarth/Metrics>
#include <osgEarth/GLUtils>
#include <osgEarth/StringUtils>
#if OSG_VERSION_GREATER_OR_EQUAL(3,5,0)
#include <osg/ContextData>
#endif

using namespace osgEarth;
using namespace osgEarth::Util;

#define LC "[ResourceReleaser] "


ResourceReleaser::ResourceReleaser()
{
    // ensure this node always gets traversed:
    this->setCullingActive(false);

    // ensure the draw runs synchronously:
    this->setDataVariance(DYNAMIC);

    // force the draw to run every frame:
    this->setUseDisplayList(false);
}

void
ResourceReleaser::push(osg::Object* object)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _toRelease.push_back(object);
}

void
ResourceReleaser::push(const ObjectList& objects)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _toRelease.reserve(_toRelease.size() + objects.size());
    for (unsigned i = 0; i<objects.size(); ++i)
        _toRelease.push_back(objects[i].get());
}

void
ResourceReleaser::drawImplementation(osg::RenderInfo& ri) const
{
    OE_GL_ZONE;
    releaseGLObjects(ri.getState());
}

void
ResourceReleaser::releaseGLObjects(osg::State* state) const
{
    OE_PROFILING_ZONE;
    if (!_toRelease.empty())
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_toRelease.empty())
        {
            for (ObjectList::const_iterator i = _toRelease.begin(); i != _toRelease.end(); ++i)
            {
                osg::Object* object = i->get();
                object->releaseGLObjects(state);
            }
            OE_PROFILING_ZONE_TEXT(Stringify() << "Released " << _toRelease.size());
            OE_DEBUG << LC << "Released " << _toRelease.size() << " objects\n";
            _toRelease.clear();
        }
    }
}
