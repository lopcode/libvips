/* extract an area and/or a set of bands
 *
 * Copyright: 1990, J. Cupitt
 *
 * Author: J. Cupitt
 * Written on: 12/02/1990
 * Modified on: 4/6/92, J.Cupitt
 *	- speed up! why wasn't this done before? Why am I stupid?
 *	- layout, messages fixed
 * now extracts IM_CODING_LABQ to IM_CODING_LABQ file: K.Martinez 1/7/93
 * 2/7/93 JC
 *	- adapted for partial v2
 *	- ANSIfied
 * 7/7/93 JC
 *	- behaviour for IM_CODING_LABQ fixed
 *	- better messages
 * 7/10/94 JC
 *	- new IM_NEW()
 * 22/2/95 JC
 *	- new use of im_region_region()
 * 6/7/98 JC
 *	- im_extract_area() and im_extract_band() added
 * 11/7/01 JC
 *	- im_extract_band() now numbers from zero
 * 7/11/01 JC
 *	- oh what pain, im_extract now numbers bands from zero as well
 * 6/9/02 JC
 *	- zero xoff/yoff for extracted area
 * 14/4/04 JC
 *	- nope, -ve the origin
 * 17/7/04
 *	- added im_extract_bands(), remove many bands from image
 * 24/3/09
 * 	- added IM_CODING_RAD support
 * 29/1/10
 * 	- cleanups
 * 	- gtkdoc
 * 26/10/11
 * 	- redone as a class
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define VIPS_DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vips/vips.h>
#include <vips/internal.h>
#include <vips/debug.h>

#include "conversion.h"

/**
 * VipsExtractArea:
 * @input: input image
 * @output: output image
 * @left: left edge of area to extract
 * @top: top edge of area to extract
 * @width: width of area to extract
 * @height: height of area to extract
 *
 * Extract an area from an image.
 * Extracting outside @input will trigger an error.
 *
 * See also: VipsExtractBand().
 */

typedef struct _VipsExtractArea {
	VipsConversion parent_instance;

	/* The input image.
	 */
	VipsImage *input;

	int left;
	int top;
	int width;
	int height;

} VipsExtractArea;

typedef VipsConversionClass VipsExtractAreaClass;

G_DEFINE_TYPE( VipsExtractArea, vips_extract_area, VIPS_TYPE_CONVERSION );

/* Extract an area. Can just use pointers.
 */
static int
vips_extract_area_gen( VipsRegion *or, void *seq, void *a, void *b, 
	gboolean *stop )
{
	VipsRegion *ir = (VipsRegion *) seq;
	VipsExtractArea *extract = (VipsExtractArea *) b;
	VipsRect iarea;

	/* Ask for input we need. Translate from demand in or's space to
	 * demand in ir's space.
	 */
	iarea = or->valid;
	iarea.left += extract->left;
	iarea.top += extract->top;
	if( vips_region_prepare( ir, &iarea ) )
		return( -1 );

	/* Attach or to ir.
	 */
	if( vips_region_region( or, ir, &or->valid, iarea.left, iarea.top ) )
		return( -1 );
	
	return( 0 );
}

static int
vips_extract_area_build( VipsObject *object )
{
	VipsConversion *conversion = VIPS_CONVERSION( object );
	VipsExtractArea *extract = (VipsExtractArea *) object;

	if( VIPS_OBJECT_CLASS( vips_extract_area_parent_class )->build( object ) )
		return( -1 );

	if( extract->left + extract->width > extract->input->Xsize ||
		extract->top + extract->height > extract->input->Ysize ||
		extract->left < 0 || extract->top < 0 ||
		extract->width <= 0 || extract->height <= 0 ) {
		im_error( "VipsExtractArea", "%s", _( "bad extract area" ) );
		return( -1 );
	}

	if( vips_image_pio_input( extract->input ) || 
		vips_image_pio_output( conversion->output ) ||
		vips_check_coding_known( "VipsExtractArea", extract->input ) )  
		return( -1 );

	if( vips_image_copy_fields( conversion->output, extract->input ) )
		return( -1 );
	vips_demand_hint( conversion->output, 
		VIPS_DEMAND_STYLE_THINSTRIP, extract->input, NULL );

        conversion->output->Xsize = extract->width;
        conversion->output->Ysize = extract->height;
        conversion->output->Xoffset = -extract->left;
        conversion->output->Yoffset = -extract->top;

	if( vips_image_generate( conversion->output,
		vips_start_one, vips_extract_area_gen, vips_stop_one, 
		extract->input, extract ) )
		return( -1 );

	return( 0 );
}

/* xy range we sanity check on ... just to stop crazy numbers from 1/0 etc.
 * causing g_assert() failures later.
 */
#define RANGE (100000000)

static void
vips_extract_area_class_init( VipsExtractAreaClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *vobject_class = VIPS_OBJECT_CLASS( class );

	VIPS_DEBUG_MSG( "vips_extract_area_class_init\n" );

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	vobject_class->nickname = "extract_area";
	vobject_class->description = _( "extract an area from an image" );
	vobject_class->build = vips_extract_area_build;

	VIPS_ARG_IMAGE( class, "input", 0, 
		_( "Input" ), 
		_( "Input image" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsExtractArea, input ) );

	VIPS_ARG_INT( class, "left", 2, 
		_( "Left" ), 
		_( "Left edge of extract area" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsExtractArea, left ),
		-RANGE, RANGE, 0 );

	VIPS_ARG_INT( class, "top", 3, 
		_( "Top" ), 
		_( "Top edge of extract area" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsExtractArea, top ),
		-RANGE, RANGE, 0 );

	VIPS_ARG_INT( class, "width", 4, 
		_( "Width" ), 
		_( "Width of extract area" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsExtractArea, width ),
		1, RANGE, 1 );

	VIPS_ARG_INT( class, "height", 5, 
		_( "Height" ), 
		_( "Height of extract area" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsExtractArea, height ),
		1, RANGE, 1 );

}

static void
vips_extract_area_init( VipsExtractArea *extract )
{
}

int
vips_extract_area( VipsImage *input, VipsImage **output, 
	int left, int top, int width, int height, ... )
{
	va_list ap;
	int result;

	va_start( ap, height );
	result = vips_call_split( "extract_area", ap, input, output, 
		left, top, width, height );
	va_end( ap );

	return( result );
}

/**
 * VipsExtractBand:
 * @input: input image
 * @output: output image
 * @band: band to extract
 * @n: number of bands to extract
 *
 * Extract a band or bands from an image. Extracting out of range is an error.
 *
 * See also: VipsExtractArea().
 */

typedef struct _VipsExtractBand {
	VipsConversion parent_instance;

	/* The input image.
	 */
	VipsImage *input;

	int band;
	int n;

} VipsExtractBand;

typedef VipsConversionClass VipsExtractBandClass;

G_DEFINE_TYPE( VipsExtractBand, vips_extract_band, VIPS_TYPE_CONVERSION );

static int
vips_extract_band_gen( VipsRegion *or, void *seq, void *a, void *b, 
	gboolean *stop )
{
	VipsRegion *ir = (VipsRegion *) seq;
	VipsExtractBand *extract = (VipsExtractBand *) b;
	VipsRect *r = &or->valid;
	int es = VIPS_IMAGE_SIZEOF_ELEMENT( ir->im );	
	int ipel = VIPS_IMAGE_SIZEOF_PEL( ir->im );
	int opel = VIPS_IMAGE_SIZEOF_PEL( or->im );

	char *p, *q;
	int x, y, z;

	if( vips_region_prepare( ir, r ) )
		return( -1 );

	for( y = 0; y < r->height; y++ ) {
		p = VIPS_REGION_ADDR( ir, r->left, r->top + y ) + 
			extract->band * es;
		q = VIPS_REGION_ADDR( or, r->left, r->top + y );

		for( x = 0; x < r->width; x++ ) {
			for( z = 0; z < opel; z++ )
				q[z] = p[z];

			p += ipel;
			q += opel;
		}
	}

	return( 0 );
}

static int
vips_extract_band_build( VipsObject *object )
{
	VipsConversion *conversion = VIPS_CONVERSION( object );
	VipsExtractBand *extract = (VipsExtractBand *) object;

	if( VIPS_OBJECT_CLASS( vips_extract_band_parent_class )->build( object ) )
		return( -1 );

	if( extract->band + extract->n > extract->input->Bands ) {
		im_error( "VipsExtractBand", "%s", _( "bad extract band" ) );
		return( -1 );
	}

	if( vips_image_pio_input( extract->input ) || 
		vips_image_pio_output( conversion->output ) ||
		vips_check_coding_known( "VipsExtractBand", extract->input ) ) 
		return( -1 );

	if( vips_image_copy_fields( conversion->output, extract->input ) )
		return( -1 );
	vips_demand_hint( conversion->output, 
		VIPS_DEMAND_STYLE_THINSTRIP, extract->input, NULL );

        conversion->output->Bands = extract->n;

	if( vips_image_generate( conversion->output,
		vips_start_one, vips_extract_band_gen, vips_stop_one, 
		extract->input, extract ) )
		return( -1 );

	return( 0 );
}

static void
vips_extract_band_class_init( VipsExtractBandClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *vobject_class = VIPS_OBJECT_CLASS( class );

	VIPS_DEBUG_MSG( "vips_extract_band_class_init\n" );

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	vobject_class->nickname = "extract_band";
	vobject_class->description = _( "extract band from an image" );
	vobject_class->build = vips_extract_band_build;

	VIPS_ARG_IMAGE( class, "input", 0, 
		_( "Input" ), 
		_( "Input image" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsExtractBand, input ) );

	VIPS_ARG_INT( class, "band", 3, 
		_( "Band" ), 
		_( "Band to extract" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsExtractBand, band ),
		0, RANGE, 0 );

	VIPS_ARG_INT( class, "n", 4, 
		_( "n" ), 
		_( "Number of bands to extract" ),
		VIPS_ARGUMENT_OPTIONAL_INPUT,
		G_STRUCT_OFFSET( VipsExtractBand, n ),
		1, RANGE, 1 );

}

static void
vips_extract_band_init( VipsExtractBand *extract )
{
}

int
vips_extract_band( VipsImage *input, VipsImage **output, int band, ... )
{
	va_list ap;
	int result;

	va_start( ap, band );
	result = vips_call_split( "extract_band", ap, input, output, band );
	va_end( ap );

	return( result );
}
