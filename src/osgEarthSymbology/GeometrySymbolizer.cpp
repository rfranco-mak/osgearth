/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2009 Pelican Ventures, Inc.
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
#include <osgEarthSymbology/GeometrySymbolizer>
#include <osgEarthSymbology/SymbolPoint>
#include <osgEarthFeatures/Feature>
#include <osgUtil/Tessellator>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/Geode>

using namespace osgEarth::Symbology;
using namespace osgEarth::Features;

GeometrySymbolizer::GeometrySymbolizer()
{
}


bool 
GeometrySymbolizer::update(FeatureDataSet* dataSet,
                           const osgEarth::Symbology::Style& style,
                           osg::Group* attachPoint,
                           SymbolizerContext* context )
{
    if (!dataSet || !attachPoint)
        return false;

    osg::ref_ptr<osgEarth::Features::FeatureCursor> cursor = dataSet->createCursor();
    if (!cursor)
        return false;

    osg::ref_ptr<osg::Group> newSymbolized = new osg::Group;
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    newSymbolized->addChild(geode.get());

    osgEarth::Features::Feature* feature = 0;
    while( cursor->hasMore() ) 
    {
        feature = cursor->nextFeature();
        if (!feature)
            continue;

        Geometry* geometry = feature->getGeometry();

        osg::ref_ptr<osg::Geometry> osgGeom = new osg::Geometry;
        osg::PrimitiveSet::Mode primMode = osg::PrimitiveSet::POINTS;

        osg::Vec4 color = osg::Vec4(1.0, 0.0, 1.0, 1.);

        const Symbol* symbol = style.getSymbol(geometry->getType());
        switch( symbol->getType())
        {
        case Symbol::POINT:
        {
            primMode = osg::PrimitiveSet::POINTS;
            const SymbolPoint* sp = dynamic_cast<const SymbolPoint*>(symbol);
            color = sp->getColor();
        }
        break;

        case Symbol::LINESTRING:
            primMode = osg::PrimitiveSet::LINE_STRIP;
            break;

        case Symbol::RING:
            primMode = osg::PrimitiveSet::LINE_LOOP;
            break;

        case Symbol::POLYGON:
            primMode = osg::PrimitiveSet::LINE_LOOP; // loop will tessellate into polys
            break;
        }

        osg::Material* material = new osg::Material;
        material->setDiffuse(osg::Material::FRONT_AND_BACK, color);

        if ( symbol->getType() == Symbol::POLYGON && geometry->getType() == Geometry::TYPE_POLYGON && static_cast<Polygon*>(geometry)->getHoles().size() > 0 )
        {
            Polygon* poly = static_cast<Polygon*>(geometry);
            int totalPoints = poly->getTotalPointCount();
            osg::Vec3Array* allPoints = new osg::Vec3Array( totalPoints );
            int offset = 0;
            for( RingCollection::const_iterator h = poly->getHoles().begin(); h != poly->getHoles().end(); ++h )
            {
                Geometry* hole = h->get();
                std::copy( hole->begin(), hole->end(), allPoints->begin() + offset );
                osgGeom->addPrimitiveSet( new osg::DrawArrays( primMode, offset, hole->size() ) );
                offset += hole->size();
            }
            osgGeom->setVertexArray( allPoints );
        }
        else
        {
            osgGeom->setVertexArray( geometry->toVec3Array() );
            osgGeom->addPrimitiveSet( new osg::DrawArrays( primMode, 0, geometry->size() ) );
        }

        // tessellate all polygon geometries. Tessellating each geometry separately
        // with TESS_TYPE_GEOMETRY is much faster than doing the whole bunch together
        // using TESS_TYPE_DRAWABLE.

        if ( symbol->getType() == Symbol::POLYGON)
        {
            osgUtil::Tessellator tess;
            tess.setTessellationType( osgUtil::Tessellator::TESS_TYPE_GEOMETRY );
            tess.setWindingType( osgUtil::Tessellator::TESS_WINDING_POSITIVE );
            tess.retessellatePolygons( *osgGeom );
        }
        osgGeom->getOrCreateStateSet()->setAttributeAndModes(material);
        geode->addDrawable(osgGeom);

    }

    if (geode->getNumDrawables()) 
    {
        attachPoint->addChild(newSymbolized.get());
        return true;
    }

    return false;
}
