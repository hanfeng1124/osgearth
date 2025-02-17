/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
 * Copyright 2020 Pelican Mapping
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
#include "PagedNode"
#include "GLUtils"
#include "NodeUtils"
#include "Progress"

#define LC "[PagedNode] "

using namespace osgEarth;
using namespace osgEarth::Util;

#define PAGEDNODE_ARENA_NAME "oe.nodepager"

PagedNode2::PagedNode2()
{
    _job.name = (typeid(*this).name());
    _job.pool = jobs::get_pool(PAGEDNODE_ARENA_NAME);
}

PagedNode2::~PagedNode2()
{
    //nop
}

bool
PagedNode2::isHighestResolution() const
{
    return getNumChildren() == 0;
}

void
PagedNode2::setLoadFunction(const Loader& value)
{
    _load_function = value;
}

void
PagedNode2::traverse(osg::NodeVisitor& nv)
{
    // locate the paging manager if there is one
    if (_pagingManager == nullptr)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_pagingManager == nullptr) // double check
        {
            osg::ref_ptr<PagingManager> pm;
            if (ObjectStorage::get(&nv, pm))
                _pagingManager = pm.get();
        }
    }

    if (nv.getTraversalMode() == nv.TRAVERSE_ACTIVE_CHILDREN)
    {
        if (nv.getVisitorType() == nv.CULL_VISITOR)
        {
            bool inRange = false;
            //float priority = 0.0f;

            if (_useRange) // meters
            {
                float range = std::max(0.0f, nv.getDistanceToViewPoint(getBound().center(), true) - getBound().radius());
                inRange = (range >= _minRange && range <= _maxRange);
                _lastPriority = -range * _priorityScale;
            }
            else // pixels
            {
                osg::CullStack* cullStack = nv.asCullStack();
                if (cullStack != nullptr && cullStack->getLODScale() > 0.0f)
                {
                    float pixels = cullStack->clampedPixelSize(getBound()) / cullStack->getLODScale();
                    inRange = (pixels >= _minPixels && pixels <= _maxPixels);
                    _lastPriority = pixels * _priorityScale;
                }
            }

            if (inRange)
            {
                if (_load_function && _loaded.empty() && !_loadGate.exchange(true))
                {
                    startLoad(_lastPriority, &nv);
                }

                // traverse children
                traverseChildren(nv);

                // stay alive
                touch();
            }
            else
            {
                // child out of range; just accept static children
                auto paged_child = _merged.has_value(true) ? _loaded.value() : nullptr;

                for (auto& child : _children)
                {
                    if (child.get() != paged_child)
                    {
                        child->accept(nv);
                    }
                }
            }
        }
        else
        {
            // Only traverse the highest res children otherwise
            traverseChildren(nv);
        }
    }

    else if (nv.getTraversalMode() == nv.TRAVERSE_ALL_CHILDREN)
    {
        for (auto& child : _children)
        {
            if (child.valid())
                child->accept(nv);
        }
    }
}

void
PagedNode2::traverseChildren(osg::NodeVisitor& nv)
{
    if (_refinePolicy == REFINE_REPLACE && _merged.has_value(true))
    {
        _loaded.value()->accept(nv);
    }
    else
    {
        for (auto& child : _children)
        {
            child->accept(nv);
        }
    }
}

void
PagedNode2::touch()
{
    // tell the paging manager this node is still alive
    // (and should not be removed from the scene graph)
    if (_pagingManager)
    {
        _token = _pagingManager->use(this, _token);
    }
}

bool
PagedNode2::merge(int revision)
{
    // Check the revision, b/c it is possible for a node in the merge queue
    // to be expired before it pops to the front of the merge queue and
    // this method gets invoked.
    if (_revision == revision)
    {
        // This is called from PagingManager.
        // We're in the UPDATE traversal.
        OE_SOFT_ASSERT_AND_RETURN(_loaded.available(), false);
        OE_SOFT_ASSERT_AND_RETURN(_loaded.value().valid(), false);
        OE_SOFT_ASSERT_AND_RETURN(_loaded.value()->getNumParents() == 0, false);

        addChild(_loaded.value());

        if (_callbacks.valid())
            _callbacks->firePostMergeNode(_loaded.value().get());

        _merged.resolve(true);
        return true;
    }
    else
    {
        _merged.resolve(false);
        return false;
    }
}

osg::BoundingSphere
PagedNode2::computeBound() const
{
    if (_userBS.isSet() && _userBS->radius() >= 0.0f)
    {
        return _userBS.get();
    }

    else
    {
        osg::BoundingSphere bs = osg::Group::computeBound();

        if (!_merged.available() && _loaded.available() && _loaded.value().valid())
        {
            bs.expandBy(_loaded.value()->computeBound());
        }

        return bs;
    }
}

void
PagedNode2::startLoad(float priority, const osg::Object* host)
{
    // Load the asynchronous node.
    Loader load_function(_load_function);
    osg::observer_ptr<SceneGraphCallbacks> callbacks_weak(_callbacks);
    bool preCompile = _preCompile;
    auto pnode_weak = osg::observer_ptr<PagedNode2>(this);

    jobs::context context;
    context.pool = jobs::get_pool(PAGEDNODE_ARENA_NAME);
    context.priority = [pnode_weak]() {
            osg::ref_ptr<PagedNode2> pnode;
            return pnode_weak.lock(pnode) ? pnode->_lastPriority : -FLT_MAX;
        };

    // Job to load the child node and compile its GL objects if necessary
    auto load_and_compile_job = [load_function, callbacks_weak, preCompile, host](auto& promise)
        {
            osg::ref_ptr<osg::Node> result;

            osg::ref_ptr<ProgressCallback> progress = new ProgressCallback(&promise);

            // invoke the loader function
            result = load_function(progress.get());

            // Fire any pre-merge callbacks
            if (result.valid())
            {
                osg::ref_ptr<SceneGraphCallbacks> callbacks;
                if (callbacks_weak.lock(callbacks))
                {
                    callbacks->firePreMergeNode(result.get());
                }

                if (result->getBound().valid())
                {
                    // Collect the GL objects for later compilation.
                    // Don't waste precious ICO time doing this later
                    GLObjectsCompiler compiler;
                    auto state = compiler.collectState(result.get());
                    compiler.requestIncrementalCompile(result, state.get(), host, promise);
                    return;
                }
            }
                
            promise.resolve(result);
        };

    // Job to request a scene graph merge.
    // The promise is unused in the job, but we plan to manually resolve it in
    // PagedNode::merge().
    auto merge_job = [pnode_weak](const osg::ref_ptr<osg::Node>& node, auto& promise)
        {
            osg::ref_ptr<PagedNode2> pnode;
            if (pnode_weak.lock(pnode) && pnode->_loaded.available() && pnode->_pagingManager)
            {
                pnode->_pagingManager->merge(pnode);
                pnode->dirtyBound();
            }
            else promise.resolve(false);
        };

    _loaded = jobs::dispatch(load_and_compile_job, _loaded, context);

    _merged = _loaded.then_dispatch<bool>(merge_job, context);
}

void PagedNode2::unload()
{
    if (_merged.has_value(true))
    {
        removeChild(_loaded.value());
    }

    //_compiled.reset();
    _loaded.reset();
    _merged.reset();

    _loadGate.exchange(false);
    _token = nullptr;

    // prevents a node in the PagingManager's merge queue from being merged with old data.
    _revision++;
}

bool PagedNode2::isLoaded() const
{
    return _merged.has_value(true);
}

PagingManager::PagingManager()
{
    setCullingActive(false);
    ADJUST_UPDATE_TRAV_COUNT(this, +1);

    auto pool = jobs::get_pool(PAGEDNODE_ARENA_NAME);
    pool->set_concurrency(4u);
    _metrics = pool->metrics();
}

PagingManager::~PagingManager()
{
    if (_mergeQueue.size() > 0)
    {
        _metrics->postprocessing.exchange(_metrics->postprocessing - _mergeQueue.size());
    }
}

void
PagingManager::traverse(osg::NodeVisitor& nv)
{
    ObjectStorage::set(&nv, this);

    if (nv.getVisitorType() == nv.CULL_VISITOR)
    {
        _newFrame.exchange(true);
    }

    else if (nv.getVisitorType() == nv.UPDATE_VISITOR)
    {
        if (_newFrame.exchange(false) == true)
        {
            update();
        }
    }

    osg::Group::traverse(nv);

    if (nv.getVisitorType() == nv.CULL_VISITOR)
    {
        // After culling is complete, update all of the ranges for all of the node
        scoped_lock_if lock(_trackerMutex, _threadsafe);

        for (auto& entry : _tracker._list)
        {
            if (entry._data.valid())
            {               
                float range = std::max(0.0f, nv.getDistanceToViewPoint(entry._data->getBound().center(), true) - entry._data->getBound().radius());
                entry._data->_lastRange = std::min(entry._data->_lastRange, range);
            }
        }
    }
}

void
PagingManager::merge(PagedNode2* host)
{
    scoped_lock_if lock(_mergeMutex, _threadsafe);

    ToMerge toMerge;
    toMerge._node = host;
    toMerge._revision = host->_revision;
    _mergeQueue.push(std::move(toMerge));
    _metrics->postprocessing++;
}

void
PagingManager::update()
{
    scoped_lock_if lock(_trackerMutex, _threadsafe);

    _tracker.flush(
        _mergesPerFrame,
        [this](osg::ref_ptr<PagedNode2>& node) -> bool
        {
            // if the node is no longer in the scene graph, expunge it
            if (node->referenceCount() == 1)
            {
                return true;
            }

            // Don't expire nodes that are still within range even if they haven't passed cull.
            if (node->_lastRange < node->getMaxRange())
            {
                return false;
            }

            if (node->getAutoUnload())
            {
                node->unload();
                return true;
            }
            return false;
        });

    // Reset the lastRange on the nodes for the next frame.
    for (auto& entry : _tracker._list)
    {
        if (entry._data.valid())
        {
            entry._data->_lastRange = FLT_MAX;
        }
    }

    // Handle merges
    unsigned count = 0u;
    while (_mergeQueue.empty() == false && count < _mergesPerFrame)
    {
        ToMerge& front = _mergeQueue.front();
        osg::ref_ptr<PagedNode2> next;
        if (front._node.lock(next))
        {
            if (next->merge(front._revision))
                ++count;
        }
        _mergeQueue.pop();
        _metrics->postprocessing--;
    }
}
