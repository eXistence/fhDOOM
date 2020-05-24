/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "RenderProgram.h"
#include "RenderList.h"
#include "Framebuffer.h"
#include "ImageData.h"
#include "ImageProgram.h"
#include "Sampler.h"
#include "ImmediateMode.h"
#include <mutex>

// Vista OpenGL wrapper check
#ifdef _WIN32
#include "../sys/win32/win_local.h"
#endif

// functions that are not called every frame

glconfig_t	glConfig;

void GfxInfo_f( const idCmdArgs& args );

idCVar r_glDebugOutput( "r_glDebugOutput", "0", CVAR_RENDERER | CVAR_INTEGER | CVAR_ARCHIVE, "print OpenGL debug output: 0 = Off, 1 = Asynchronous, 2 = Synchronous" );
idCVar r_useLightPortalFlow( "r_useLightPortalFlow", "1", CVAR_RENDERER | CVAR_BOOL, "use a more precise area reference determination" );
idCVar r_multiSamples( "r_multiSamples", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "number of antialiasing samples" );
idCVar r_mode( "r_mode", "5", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_INTEGER, "video mode number" );
idCVar r_displayRefresh( "r_displayRefresh", "0", CVAR_RENDERER | CVAR_INTEGER | CVAR_NOCHEAT, "optional display refresh rate option for vid mode", 0.0f, 200.0f );
idCVar r_fullscreen( "r_fullscreen", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "0 = windowed, 1 = full screen" );
idCVar r_customWidth( "r_customWidth", "720", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "custom screen width. set r_mode to -1 to activate" );
idCVar r_customHeight( "r_customHeight", "486", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "custom screen height. set r_mode to -1 to activate" );
idCVar r_singleTriangle( "r_singleTriangle", "0", CVAR_RENDERER | CVAR_BOOL, "only draw a single triangle per primitive" );
idCVar r_checkBounds( "r_checkBounds", "0", CVAR_RENDERER | CVAR_BOOL, "compare all surface bounds with precalculated ones" );

idCVar r_useConstantMaterials( "r_useConstantMaterials", "1", CVAR_RENDERER | CVAR_BOOL, "use pre-calculated material registers if possible" );
idCVar r_useSilRemap( "r_useSilRemap", "1", CVAR_RENDERER | CVAR_BOOL, "consider verts with the same XYZ, but different ST the same for shadows" );
idCVar r_useNodeCommonChildren( "r_useNodeCommonChildren", "1", CVAR_RENDERER | CVAR_BOOL, "stop pushing reference bounds early when possible" );
idCVar r_useShadowProjectedCull( "r_useShadowProjectedCull", "1", CVAR_RENDERER | CVAR_BOOL, "discard triangles outside light volume before shadowing" );
idCVar r_useShadowSurfaceScissor( "r_useShadowSurfaceScissor", "1", CVAR_RENDERER | CVAR_BOOL, "scissor shadows by the scissor rect of the interaction surfaces" );
idCVar r_useInteractionTable( "r_useInteractionTable", "1", CVAR_RENDERER | CVAR_BOOL, "create a full entityDefs * lightDefs table to make finding interactions faster" );
idCVar r_useTurboShadow( "r_useTurboShadow", "1", CVAR_RENDERER | CVAR_BOOL, "use the infinite projection with W technique for dynamic shadows" );
idCVar r_useTwoSidedStencil( "r_useTwoSidedStencil", "1", CVAR_RENDERER | CVAR_BOOL, "do stencil shadows in one pass with different ops on each side" );
idCVar r_useDeferredTangents( "r_useDeferredTangents", "1", CVAR_RENDERER | CVAR_BOOL, "defer tangents calculations after deform" );
idCVar r_useCachedDynamicModels( "r_useCachedDynamicModels", "1", CVAR_RENDERER | CVAR_BOOL, "cache snapshots of dynamic models" );

idCVar r_useIndexBuffers( "r_useIndexBuffers", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "use ARB_vertex_buffer_object for indexes", 0, 1, idCmdSystem::ArgCompletion_Integer<0,1>  );

idCVar r_useStateCaching( "r_useStateCaching", "1", CVAR_RENDERER | CVAR_BOOL, "avoid redundant state changes in GL_*() calls" );
idCVar r_useInfiniteFarZ( "r_useInfiniteFarZ", "1", CVAR_RENDERER | CVAR_BOOL, "use the no-far-clip-plane trick" );

idCVar r_znear( "r_znear", "1", CVAR_RENDERER | CVAR_FLOAT, "near Z clip plane distance", 0.001f, 200.0f );

idCVar r_finish( "r_finish", "0", CVAR_RENDERER | CVAR_BOOL, "force a call to glFinish() every frame" );
idCVar r_swapInterval( "r_swapInterval", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "changes wglSwapIntarval" );

idCVar r_gamma( "r_gamma", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "changes gamma tables", 0.5f, 3.0f );
idCVar r_brightness( "r_brightness", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "changes gamma tables", 0.5f, 2.0f );

idCVar r_jitter( "r_jitter", "0", CVAR_RENDERER | CVAR_BOOL, "randomly subpixel jitter the projection matrix" );

idCVar r_skipSuppress( "r_skipSuppress", "0", CVAR_RENDERER | CVAR_BOOL, "ignore the per-view suppressions" );
idCVar r_skipPostProcess( "r_skipPostProcess", "0", CVAR_RENDERER | CVAR_BOOL, "skip all post-process renderings" );
idCVar r_skipLightScale( "r_skipLightScale", "0", CVAR_RENDERER | CVAR_BOOL, "don't do any post-interaction light scaling, makes things dim on low-dynamic range cards" );
idCVar r_skipInteractions( "r_skipInteractions", "0", CVAR_RENDERER | CVAR_BOOL, "skip all light/surface interaction drawing" );
idCVar r_skipDynamicTextures( "r_skipDynamicTextures", "0", CVAR_RENDERER | CVAR_BOOL, "don't dynamically create textures" );
idCVar r_skipCopyTexture( "r_skipCopyTexture", "0", CVAR_RENDERER | CVAR_BOOL, "do all rendering, but don't actually copyTexSubImage2D" );
idCVar r_skipBackEnd( "r_skipBackEnd", "0", CVAR_RENDERER | CVAR_BOOL, "don't draw anything" );
idCVar r_skipRender( "r_skipRender", "0", CVAR_RENDERER | CVAR_BOOL, "skip 3D rendering, but pass 2D" );
idCVar r_skipRenderContext( "r_skipRenderContext", "0", CVAR_RENDERER | CVAR_BOOL, "NULL the rendering context during backend 3D rendering" );
idCVar r_skipTranslucent( "r_skipTranslucent", "0", CVAR_RENDERER | CVAR_BOOL, "skip the translucent interaction rendering" );
idCVar r_skipAmbient( "r_skipAmbient", "0", CVAR_RENDERER | CVAR_BOOL, "bypasses all non-interaction drawing" );
idCVar r_skipNewAmbient( "r_skipNewAmbient", "0", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "bypasses all vertex/fragment program ambient drawing" );
idCVar r_skipGlsl( "r_skipGlsl", "0", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "bypasses all glsl stages in materials" );
idCVar r_skipBlendLights( "r_skipBlendLights", "0", CVAR_RENDERER | CVAR_BOOL, "skip all blend lights" );
idCVar r_skipFogLights( "r_skipFogLights", "0", CVAR_RENDERER | CVAR_BOOL, "skip all fog lights" );
idCVar r_skipDeforms( "r_skipDeforms", "0", CVAR_RENDERER | CVAR_BOOL, "leave all deform materials in their original state" );
idCVar r_skipFrontEnd( "r_skipFrontEnd", "0", CVAR_RENDERER | CVAR_BOOL, "bypasses all front end work, but 2D gui rendering still draws" );
idCVar r_skipUpdates( "r_skipUpdates", "0", CVAR_RENDERER | CVAR_BOOL, "1 = don't accept any entity or light updates, making everything static" );
idCVar r_skipOverlays( "r_skipOverlays", "0", CVAR_RENDERER | CVAR_BOOL, "skip overlay surfaces" );
idCVar r_skipSpecular( "r_skipSpecular", "0", CVAR_RENDERER | CVAR_BOOL | CVAR_CHEAT | CVAR_ARCHIVE, "use black for specular1" );
idCVar r_skipBump( "r_skipBump", "0", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "uses a flat surface instead of the bump map" );
idCVar r_skipDiffuse( "r_skipDiffuse", "0", CVAR_RENDERER | CVAR_BOOL, "use black for diffuse" );
idCVar r_skipROQ( "r_skipROQ", "0", CVAR_RENDERER | CVAR_BOOL, "skip ROQ decoding" );

idCVar r_glslReplaceArb2( "r_glslReplaceArb2", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "replace ARB2 shaders by GLSL shaders if possible" );
idCVar r_glCoreProfile( "r_glCoreProfile", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "use OpenGL core profile" );

idCVar r_ignore( "r_ignore", "0", CVAR_RENDERER, "used for random debugging without defining new vars" );
idCVar r_ignore2( "r_ignore2", "0", CVAR_RENDERER, "used for random debugging without defining new vars" );
idCVar r_ignore3( "r_ignore3", "0", CVAR_RENDERER, "used for random debugging without defining new vars" );
idCVar r_ignore4( "r_ignore4", "0", CVAR_RENDERER, "used for random debugging without defining new vars" );
idCVar r_usePreciseTriangleInteractions( "r_usePreciseTriangleInteractions", "0", CVAR_RENDERER | CVAR_BOOL, "1 = do winding clipping to determine if each ambiguous tri should be lit" );
idCVar r_useCulling( "r_useCulling", "2", CVAR_RENDERER | CVAR_INTEGER, "0 = none, 1 = sphere, 2 = sphere + box", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_useLightCulling( "r_useLightCulling", "3", CVAR_RENDERER | CVAR_INTEGER, "0 = none, 1 = box, 2 = exact clip of polyhedron faces, 3 = also areas", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_useLightScissors( "r_useLightScissors", "1", CVAR_RENDERER | CVAR_BOOL, "1 = use custom scissor rectangle for each light" );
idCVar r_useClippedLightScissors( "r_useClippedLightScissors", "1", CVAR_RENDERER | CVAR_INTEGER, "0 = full screen when near clipped, 1 = exact when near clipped, 2 = exact always", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_useEntityCulling( "r_useEntityCulling", "1", CVAR_RENDERER | CVAR_BOOL, "0 = none, 1 = box" );
idCVar r_useEntityScissors( "r_useEntityScissors", "0", CVAR_RENDERER | CVAR_BOOL, "1 = use custom scissor rectangle for each entity" );
idCVar r_useInteractionCulling( "r_useInteractionCulling", "1", CVAR_RENDERER | CVAR_BOOL, "1 = cull interactions" );
idCVar r_useInteractionScissors( "r_useInteractionScissors", "2", CVAR_RENDERER | CVAR_INTEGER, "1 = use a custom scissor rectangle for each shadow interaction, 2 = also crop using portal scissors", -2, 2, idCmdSystem::ArgCompletion_Integer<-2,2> );
idCVar r_useShadowCulling( "r_useShadowCulling", "1", CVAR_RENDERER | CVAR_BOOL, "try to cull shadows from partially visible lights" );
idCVar r_useFrustumFarDistance( "r_useFrustumFarDistance", "0", CVAR_RENDERER | CVAR_FLOAT, "if != 0 force the view frustum far distance to this distance" );
idCVar r_clear( "r_clear", "2", CVAR_RENDERER, "force screen clear every frame, 1 = purple, 2 = black, 'r g b' = custom" );
idCVar r_offsetFactor( "r_offsetfactor", "0", CVAR_RENDERER | CVAR_FLOAT, "polygon offset parameter" );
idCVar r_offsetUnits( "r_offsetunits", "-600", CVAR_RENDERER | CVAR_FLOAT, "polygon offset parameter" );
idCVar r_shadowPolygonOffset( "r_shadowPolygonOffset", "-1", CVAR_RENDERER | CVAR_FLOAT, "bias value added to depth test for stencil shadow drawing" );
idCVar r_shadowPolygonFactor( "r_shadowPolygonFactor", "0", CVAR_RENDERER | CVAR_FLOAT, "scale value for stencil shadow drawing" );
idCVar r_skipSubviews( "r_skipSubviews", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = don't render any gui elements on surfaces" );
idCVar r_skipGuiShaders( "r_skipGuiShaders", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = skip all gui elements on surfaces, 2 = skip drawing but still handle events, 3 = draw but skip events", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_skipParticles( "r_skipParticles", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = skip all particle systems", 0, 1, idCmdSystem::ArgCompletion_Integer<0,1> );
idCVar r_subviewOnly( "r_subviewOnly", "0", CVAR_RENDERER | CVAR_BOOL, "1 = don't render main view, allowing subviews to be debugged" );
idCVar r_shadows( "r_shadows", "2", CVAR_RENDERER | CVAR_INTEGER  | CVAR_ARCHIVE, "enable shadows" );
idCVar r_testGamma( "r_testGamma", "0", CVAR_RENDERER | CVAR_FLOAT, "if > 0 draw a grid pattern to test gamma levels", 0, 195 );
idCVar r_testGammaBias( "r_testGammaBias", "0", CVAR_RENDERER | CVAR_FLOAT, "if > 0 draw a grid pattern to test gamma levels" );
idCVar r_testStepGamma( "r_testStepGamma", "0", CVAR_RENDERER | CVAR_FLOAT, "if > 0 draw a grid pattern to test gamma levels" );
idCVar r_lightScale( "r_lightScale", "2", CVAR_RENDERER | CVAR_FLOAT, "all light intensities are multiplied by this" );
idCVar r_lightSourceRadius( "r_lightSourceRadius", "0", CVAR_RENDERER | CVAR_FLOAT, "for soft-shadow sampling" );
idCVar r_flareSize( "r_flareSize", "1", CVAR_RENDERER | CVAR_FLOAT, "scale the flare deforms from the material def" );

idCVar r_useExternalShadows( "r_useExternalShadows", "1", CVAR_RENDERER | CVAR_INTEGER, "1 = skip drawing caps when outside the light volume, 2 = force to no caps for testing", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_useOptimizedShadows( "r_useOptimizedShadows", "1", CVAR_RENDERER | CVAR_BOOL, "use the dmap generated static shadow volumes" );
idCVar r_useScissor( "r_useScissor", "1", CVAR_RENDERER | CVAR_BOOL, "scissor clip as portals and lights are processed" );
idCVar r_useCombinerDisplayLists( "r_useCombinerDisplayLists", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_NOCHEAT, "put all nvidia register combiner programming in display lists" );
idCVar r_useDepthBoundsTest( "r_useDepthBoundsTest", "1", CVAR_RENDERER | CVAR_BOOL, "use depth bounds test to reduce shadow fill" );

idCVar r_screenFraction( "r_screenFraction", "100", CVAR_RENDERER | CVAR_INTEGER, "for testing fill rate, the resolution of the entire screen can be changed" );
idCVar r_usePortals( "r_usePortals", "1", CVAR_RENDERER | CVAR_BOOL, " 1 = use portals to perform area culling, otherwise draw everything" );
idCVar r_singleLight( "r_singleLight", "-1", CVAR_RENDERER | CVAR_INTEGER, "suppress all but one light" );
idCVar r_singleEntity( "r_singleEntity", "-1", CVAR_RENDERER | CVAR_INTEGER, "suppress all but one entity" );
idCVar r_singleSurface( "r_singleSurface", "-1", CVAR_RENDERER | CVAR_INTEGER, "suppress all but one surface on each entity" );
idCVar r_singleArea( "r_singleArea", "0", CVAR_RENDERER | CVAR_BOOL, "only draw the portal area the view is actually in" );
idCVar r_forceLoadImages( "r_forceLoadImages", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "draw all images to screen after registration" );
idCVar r_orderIndexes( "r_orderIndexes", "1", CVAR_RENDERER | CVAR_BOOL, "perform index reorganization to optimize vertex use" );
idCVar r_lightAllBackFaces( "r_lightAllBackFaces", "0", CVAR_RENDERER | CVAR_BOOL, "light all the back faces, even when they would be shadowed" );

// visual debugging info
idCVar r_showPortals( "r_showPortals", "0", CVAR_RENDERER | CVAR_BOOL, "draw portal outlines in color based on passed / not passed" );
idCVar r_showUnsmoothedTangents( "r_showUnsmoothedTangents", "0", CVAR_RENDERER | CVAR_BOOL, "if 1, put all nvidia register combiner programming in display lists" );
idCVar r_showSilhouette( "r_showSilhouette", "0", CVAR_RENDERER | CVAR_BOOL, "highlight edges that are casting shadow planes" );
idCVar r_showVertexColor( "r_showVertexColor", "0", CVAR_RENDERER | CVAR_BOOL, "draws all triangles with the solid vertex color" );
idCVar r_showUpdates( "r_showUpdates", "0", CVAR_RENDERER | CVAR_BOOL, "report entity and light updates and ref counts" );
idCVar r_showDemo( "r_showDemo", "0", CVAR_RENDERER | CVAR_BOOL, "report reads and writes to the demo file" );
idCVar r_showDynamic( "r_showDynamic", "0", CVAR_RENDERER | CVAR_BOOL, "report stats on dynamic surface generation" );
idCVar r_showLightScale( "r_showLightScale", "0", CVAR_RENDERER | CVAR_BOOL, "report the scale factor applied to drawing for overbrights" );
idCVar r_showDefs( "r_showDefs", "0", CVAR_RENDERER | CVAR_BOOL, "report the number of modeDefs and lightDefs in view" );
idCVar r_showTrace( "r_showTrace", "0", CVAR_RENDERER | CVAR_INTEGER, "show the intersection of an eye trace with the world", idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_showIntensity( "r_showIntensity", "0", CVAR_RENDERER | CVAR_BOOL, "draw the screen colors based on intensity, red = 0, green = 128, blue = 255" );
idCVar r_showImages( "r_showImages", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = show all images instead of rendering, 2 = show in proportional size", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_showSmp( "r_showSmp", "0", CVAR_RENDERER | CVAR_BOOL, "show which end (front or back) is blocking" );
idCVar r_showLights( "r_showLights", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = just print volumes numbers, highlighting ones covering the view, 2 = also draw planes of each volume, 3 = also draw edges of each volume", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showLights2( "r_showLights2", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = show light origins, 2 = show light volumes. Color means shadowmap size (red>green>blue>white, grey=none)", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_showShadows( "r_showShadows", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = visualize the stencil shadow volumes, 2 = draw filled in", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showShadowCount( "r_showShadowCount", "0", CVAR_RENDERER | CVAR_INTEGER, "colors screen based on shadow volume depth complexity, >= 2 = print overdraw count based on stencil index values, 3 = only show turboshadows, 4 = only show static shadows", 0, 4, idCmdSystem::ArgCompletion_Integer<0,4> );
idCVar r_showLightScissors( "r_showLightScissors", "0", CVAR_RENDERER | CVAR_BOOL, "show light scissor rectangles" );
idCVar r_showEntityScissors( "r_showEntityScissors", "0", CVAR_RENDERER | CVAR_BOOL, "show entity scissor rectangles" );
idCVar r_showInteractionFrustums( "r_showInteractionFrustums", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = show a frustum for each interaction, 2 = also draw lines to light origin, 3 = also draw entity bbox", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showInteractionScissors( "r_showInteractionScissors", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = show screen rectangle which contains the interaction frustum, 2 = also draw construction lines", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_showLightCount( "r_showLightCount", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = colors surfaces based on light count, 2 = also count everything through walls, 3 = also print overdraw", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showViewEntitys( "r_showViewEntitys", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = displays the bounding boxes of all view models, 2 = print index numbers" );
idCVar r_showTris( "r_showTris", "0", CVAR_RENDERER | CVAR_INTEGER, "enables wireframe rendering of the world, 1 = only draw visible ones, 2 = draw all front facing, 3 = draw all", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showSurfaceInfo( "r_showSurfaceInfo", "0", CVAR_RENDERER | CVAR_BOOL, "show surface material name under crosshair" );
idCVar r_showNormals( "r_showNormals", "0", CVAR_RENDERER | CVAR_FLOAT, "draws wireframe normals" );
idCVar r_showMemory( "r_showMemory", "0", CVAR_RENDERER | CVAR_BOOL, "print frame memory utilization" );
idCVar r_showCull( "r_showCull", "0", CVAR_RENDERER | CVAR_BOOL, "report sphere and box culling stats" );
idCVar r_showInteractions( "r_showInteractions", "0", CVAR_RENDERER | CVAR_BOOL, "report interaction generation activity" );
idCVar r_showDepth( "r_showDepth", "0", CVAR_RENDERER | CVAR_BOOL, "display the contents of the depth buffer and the depth range" );
idCVar r_showSurfaces( "r_showSurfaces", "0", CVAR_RENDERER | CVAR_BOOL, "report surface/light/shadow counts" );
idCVar r_showPrimitives( "r_showPrimitives", "0", CVAR_RENDERER | CVAR_INTEGER, "report drawsurf/index/vertex counts" );
idCVar r_showEdges( "r_showEdges", "0", CVAR_RENDERER | CVAR_BOOL, "draw the sil edges" );
idCVar r_showTexturePolarity( "r_showTexturePolarity", "0", CVAR_RENDERER | CVAR_BOOL, "shade triangles by texture area polarity" );
idCVar r_showTangentSpace( "r_showTangentSpace", "0", CVAR_RENDERER | CVAR_INTEGER, "shade triangles by tangent space, 1 = use 1st tangent vector, 2 = use 2nd tangent vector, 3 = use normal vector", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar r_showDominantTri( "r_showDominantTri", "0", CVAR_RENDERER | CVAR_BOOL, "draw lines from vertexes to center of dominant triangles" );
idCVar r_showAlloc( "r_showAlloc", "0", CVAR_RENDERER | CVAR_BOOL, "report alloc/free counts" );
idCVar r_showTextureVectors( "r_showTextureVectors", "0", CVAR_RENDERER | CVAR_FLOAT, " if > 0 draw each triangles texture (tangent) vectors" );
idCVar r_showOverDraw( "r_showOverDraw", "0", CVAR_RENDERER | CVAR_INTEGER, "1 = geometry overdraw, 2 = light interaction overdraw, 3 = geometry and light interaction overdraw", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );

idCVar r_lockSurfaces( "r_lockSurfaces", "0", CVAR_RENDERER | CVAR_BOOL, "allow moving the view point without changing the composition of the scene, including culling" );
idCVar r_useEntityCallbacks( "r_useEntityCallbacks", "1", CVAR_RENDERER | CVAR_BOOL, "if 0, issue the callback immediately at update time, rather than defering" );

idCVar r_showSkel( "r_showSkel", "0", CVAR_RENDERER | CVAR_INTEGER, "draw the skeleton when model animates, 1 = draw model with skeleton, 2 = draw skeleton only", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar r_jointNameScale( "r_jointNameScale", "0.02", CVAR_RENDERER | CVAR_FLOAT, "size of joint names when r_showskel is set to 1" );
idCVar r_jointNameOffset( "r_jointNameOffset", "0.5", CVAR_RENDERER | CVAR_FLOAT, "offset of joint names when r_showskel is set to 1" );

idCVar r_debugLineDepthTest( "r_debugLineDepthTest", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "perform depth test on debug lines" );
idCVar r_debugLineWidth( "r_debugLineWidth", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "width of debug lines" );
idCVar r_debugArrowStep( "r_debugArrowStep", "120", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "step size of arrow cone line rotation in degrees", 0, 120 );
idCVar r_debugPolygonFilled( "r_debugPolygonFilled", "1", CVAR_RENDERER | CVAR_BOOL, "draw a filled polygon" );

idCVar r_materialOverride( "r_materialOverride", "", CVAR_RENDERER, "overrides all materials", idCmdSystem::ArgCompletion_Decl<DECL_MATERIAL> );

idCVar r_debugRenderToTexture( "r_debugRenderToTexture", "0", CVAR_RENDERER | CVAR_INTEGER, "" );

// DG: let users disable the "scale menus to 4:3" hack
idCVar r_scaleMenusTo43("r_scaleMenusTo43", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "Scale menus, fullscreen videos and PDA to 4:3 aspect ratio");

idCVar r_softParticles( "r_softParticles", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "enabled soft particles");
idCVar r_defaultParticleSoftness( "r_defaultParticleSoftness", "0.35", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "");

idCVar r_windowMode("r_windowMode", "-1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "");
idCVar r_useFramebuffer( "r_useFramebuffer", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "render everything to an offscreen buffer before blitting the final image to the screen" );
idCVar r_useDisplayResolution("r_useDisplayResolution", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "");
idCVar r_framebufferScale( "r_framebufferScale", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "" );
idCVar r_force_arb_dsa("r_force_arb_dsa", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "");

namespace {

	/*
	==================
	R_CheckPortableExtensions

	==================
	*/
	bool R_DoubleCheckExtension( const char* name ) {
		//ext check via glew does not always work!? Do it manually...
		int ext_cnt = 0;
		glGetIntegerv( GL_NUM_EXTENSIONS, &ext_cnt );

		bool ret = false;

		for (int i = 0; i < ext_cnt; ++i) {
			const char* current = (const char*)glGetStringi( GL_EXTENSIONS, i );
			if (stricmp( current, name ) == 0) {
				ret = true;
				break;
			}
		}

		if (ret) {
			common->Printf( "Check extension '%s': OK\n", name );
		}
		else {
			common->Warning( "Check extension '%s': failed", name );
		}

		return ret;
	}

	void R_CheckPortableExtensions( void ) {
		glConfig.glVersion = atof( glConfig.version_string );

		glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, (GLint *)&glConfig.maxTextureImageUnits );

		// GL_ARB_texture_compression + GL_S3_s3tc
		// DRI drivers may have GL_ARB_texture_compression but no GL_EXT_texture_compression_s3tc
		bool arb_texture_compression = R_DoubleCheckExtension( "GL_ARB_texture_compression" );
		bool ext_texture_compression_s3tc = R_DoubleCheckExtension( "GL_EXT_texture_compression_s3tc" ); //should be available!
		glConfig.textureCompressionAvailable = arb_texture_compression && ext_texture_compression_s3tc;

		// GL_EXT_texture_filter_anisotropic
		glConfig.anisotropicAvailable = R_DoubleCheckExtension( "GL_EXT_texture_filter_anisotropic" );  //should be available!
		if (glConfig.anisotropicAvailable) {
			glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.maxTextureAnisotropy );
			common->Printf( "   maxTextureAnisotropy: %f\n", glConfig.maxTextureAnisotropy );
		}
		else {
			glConfig.maxTextureAnisotropy = 1;
		}

		// GL_EXT_depth_bounds_test
		glConfig.depthBoundsTestAvailable = R_DoubleCheckExtension( "GL_EXT_depth_bounds_test" );

		glConfig.extDirectStateAccessAvailable = R_DoubleCheckExtension("GL_EXT_direct_state_access");
		if (r_force_arb_dsa.GetBool() && glConfig.extDirectStateAccessAvailable) {
			common->Printf("GL_EXT_direct_state_access disabled due to 'r_force_arb_dsa'\n");
			glConfig.extDirectStateAccessAvailable = false;
		}

		glConfig.arbDirectStateAccessAvailable = R_DoubleCheckExtension( "GL_ARB_direct_state_access" );

		if (!glConfig.arbDirectStateAccessAvailable && !glConfig.extDirectStateAccessAvailable) {
			common->Error("Missing OpenGL extension: GL_EXT_direct_state_access or GL_ARB_direct_state_access must be available!\n");
		}
	}


	/*
	====================
	R_GetModeInfo

	r_mode is normally a small non-negative integer that
	looks resolutions up in a table, but if it is set to -1,
	the values from r_customWidth, amd r_customHeight
	will be used instead.
	====================
	*/

	struct vidmode_t {
		const char *description;
		int         width, height, aspectRatio;
	};

	const int AR_4_3 = 0;
	const int AR_16_9 = 1;
	const int AR_16_10 = 2;

	const vidmode_t r_vidModes[] = {
		{ "Mode  0: 320x240", 320, 240, AR_4_3 },
		{ "Mode  1: 400x300", 400, 300, AR_4_3 },
		{ "Mode  2: 512x384", 512, 384, AR_4_3 },
		{ "Mode  3: 640x480", 640, 480, AR_4_3 },
		{ "Mode  4: 800x600", 800, 600, AR_4_3 },
		{ "Mode  5: 1024x768", 1024, 768, AR_4_3 },
		{ "Mode  6: 1152x864", 1152, 864, AR_4_3 },
		{ "Mode  7: 1280x1024", 1280, 1024, AR_4_3 },
		{ "Mode  8: 1600x1200", 1600, 1200, AR_4_3 },
		//widescreen/HD:
		{ "Mode  9: 1280x720", 1280, 720, AR_16_9 },
		{ "Mode 10: 1366x768", 1366, 768, AR_16_9 },
		{ "Mode 11: 1440x900", 1440, 900, AR_16_10 },
		{ "Mode 12: 1600x900", 1600, 900, AR_16_9 },
		{ "Mode 13: 1680x1050", 1680, 1050, AR_16_10 },
		{ "Mode 14: 1920x1080", 1920, 1080, AR_16_9 },
		{ "Mode 15: 1920x1200", 1920, 1200, AR_16_10 },
	};
	const int s_numVidModes = (sizeof( r_vidModes ) / sizeof( r_vidModes[0] ));

	std::mutex debugOutputMutex;
	struct glDebugMessage_t { char buffer[512]; };
	glDebugMessage_t debugOutputMessages[32];
	int numDebugOutputMessages = 0;
}

bool R_GetModeInfo(int &width, int &height, int &aspectRatio, int mode) {

	if (mode < -1 || mode >= s_numVidModes) {
		return false;
	}

	if (mode == -1) {
		width = r_customWidth.GetInteger();
		height = r_customHeight.GetInteger();
		aspectRatio = -1;
	}
	else {
		const vidmode_t&	vm = r_vidModes[mode];
		width = vm.width;
		height = vm.height;
		aspectRatio = vm.aspectRatio;
	}

	return true;
}

/*
====================
R_GLDebugOutput

Format and print warnings from OpenGL Debug Output to console.
====================
*/
void GLAPIENTRY R_GLDebugOutput
(
GLenum source,
GLenum type,
GLuint id,
GLenum severity,
GLsizei length,
const GLchar* message,
const GLvoid* userParam
)
{
	const char* debSource = "?source?";
	const char* debType = "?type?";
	const char* debSev = "?severity?";

	static const char* ignoredMessages[] = {
		"Program/shader state performance warning: Fragment Shader is going to be recompiled because the shader key based on GL state mismatches."
	};

	if(type == GL_DEBUG_TYPE_PERFORMANCE_ARB && strcmp(ignoredMessages[0], message) == 0)
		return;

#pragma warning( push )
#pragma warning( disable : 4996 )

	switch (source) {
	case GL_DEBUG_SOURCE_API_ARB:
		debSource = "OpenGL";
		break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
		debSource = "Window System";
		break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
		debSource = "Shader Compiler";
		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
		debSource = "Third Party";
		break;
	case GL_DEBUG_SOURCE_APPLICATION_ARB:
		debSource = "Application";
		break;
	case GL_DEBUG_SOURCE_OTHER_ARB:
		debSource = "Other";
		break;
	default:
		break;
	}

	switch (type) {
	case GL_DEBUG_TYPE_ERROR_ARB:
		debType = "ERROR";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
		debType = "DEPRECATED";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
		debType = "UNDEFINED";
		break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
		debType = "PORTABILITY";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
		debType = "PERFORMANCE";
		break;
	case GL_DEBUG_TYPE_OTHER_ARB:
		debType = "MESSAGE";
		break;
	default:
		break;
	}

	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH_ARB:
		debSev = "HIGH";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM_ARB:
		debSev = "MEDIUM";
		break;
	case GL_DEBUG_SEVERITY_LOW_ARB:
		debSev = "LOW";
		break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		debSev = "NOTE";
		break;
	default:
		break;
	}

	std::lock_guard<std::mutex> lock(debugOutputMutex);
	if (numDebugOutputMessages == sizeof(debugOutputMessages) / sizeof(debugOutputMessages[0])) {
		return;
	}
#ifdef _WIN32
	_snprintf
#else
	snprintf
#endif
	(debugOutputMessages[numDebugOutputMessages++].buffer, sizeof(debugOutputMessages[0]) - 1, "(GL %u) %s, %s, %s: %s", id, debSource, debType, debSev, message);

#pragma warning( pop )
}

void RB_PrintDebugOutput()
{
	std::lock_guard<std::mutex> lock(debugOutputMutex);
	for (int i = 0; i < numDebugOutputMessages; ++i) {
		common->Printf("%s\n", debugOutputMessages[i].buffer);
	}

	numDebugOutputMessages = 0;
}


/*
==================
R_InitOpenGL

This function is responsible for initializing a valid OpenGL subsystem
for rendering.  This is done by calling the system specific GLimp_Init,
which gives us a working OGL subsystem, then setting all necessary openGL
state, including images, vertex programs, and display lists.

Changes to the vertex cache size or smp state require a vid_restart.

If glConfig.isInitialized is false, no rendering can take place, but
all renderSystem functions will still operate properly, notably the material
and model information functions.
==================
*/
void R_InitOpenGL( void ) {
	common->Printf( "----- R_InitOpenGL -----\n" );

	if (glConfig.isInitialized) {
		common->FatalError( "R_InitOpenGL called while active" );
	}
#if _WIN32
	auto displays = Sys_GetDisplays();
	common->Printf("found %d displays:\n", displays.Num());
	for (const auto& display : displays) {
		common->Printf("  %d: %s %dx%d @ %dx%d\n", display.num, display.name, display.width, display.height, display.x, display.y);
	}
#endif

	//
	// initialize OS specific portions of the renderSystem
	//
	for (int i = 0; i < 2; i++) {
		// set the parameters we are trying
		R_GetModeInfo( glConfig.vidWidth, glConfig.vidHeight, glConfig.vidAspectRatio, r_mode.GetInteger() );

		glConfig.windowWidth = glConfig.vidWidth;
		glConfig.windowHeight = glConfig.vidHeight;

		if (r_useFramebuffer.GetBool()) {
			if (r_fullscreen.GetBool() && r_useDisplayResolution.GetBool()) {
				if (Sys_GetDisplayResolution(&glConfig.windowWidth, &glConfig.windowHeight)) {
					common->Printf("OpenGL: native display resolution: %dx%d\n", glConfig.windowWidth, glConfig.windowHeight);
				}
				else {
					common->Warning("OpenGL: failed to detect native display resolution, using: %dx%d\n", glConfig.windowWidth, glConfig.windowHeight);
				}
			}
			else if (!r_fullscreen.GetBool() && r_windowMode.GetInteger() >= 0) {
				int ignored;
				if (R_GetModeInfo(glConfig.windowWidth, glConfig.windowHeight, ignored, r_windowMode.GetInteger())) {
					common->Printf("OpenGL: window resolution: %dx%d\n", glConfig.windowWidth, glConfig.windowHeight);
				}
				else {
					common->Warning("OpenGL: failed to get window resolution, using: %dx%d\n", glConfig.windowWidth, glConfig.windowHeight);
				}
			}
		}

		glimpParms_t	parms;
		parms.width = glConfig.windowWidth;
		parms.height = glConfig.windowHeight;
		parms.fullScreen = r_fullscreen.GetBool();
		parms.displayHz = r_displayRefresh.GetInteger();
		parms.multiSamples = 1;// r_multiSamples.GetInteger();
		parms.coreProfile = r_glCoreProfile.GetBool();
		parms.majorVersion = 3;
		parms.minorVersion = 3;
		parms.debug = r_glDebugOutput.GetInteger() != 0;

		common->Printf("OpenGL: parms: width=%d height=%d (%s)\n", glConfig.windowWidth, glConfig.windowHeight, (parms.fullScreen ? "fullscreen" : "windowed"));
		common->Printf("OpenGL: parms: displayHz=%d\n", parms.displayHz);
		common->Printf("OpenGL: parms: core profile=%s (debug=%s)\n", (parms.coreProfile ? "on" : "off"), (parms.debug ? "on" : "off"));
		common->Printf("OpenGL: parms: version major=%d, minor=%d\n", parms.majorVersion, parms.minorVersion );

		if (GLimp_Init( parms )) {
			// it worked
			break;
		}

		GL_CheckErrors(true);

		if (i == 1) {
			common->FatalError( "Unable to initialize OpenGL" );
		}

		common->Printf("Failed to initialize OpenGL, set everything to 'safe mode'\n");
		r_mode.SetInteger( 5 );
		r_fullscreen.SetInteger( 1 );
		r_displayRefresh.SetInteger( 0 );
		r_multiSamples.SetInteger( 0 );
		r_glCoreProfile.SetBool( false );
		r_useFramebuffer.SetBool( false );
		r_useDisplayResolution.SetBool( false );
	}

	GL_CheckErrors(true);

	// input and sound systems need to be tied to the new window
	Sys_InitInput();
	soundSystem->InitHW();

	// get our config strings
	glConfig.vendor_string = (const char *)glGetString( GL_VENDOR );
	glConfig.renderer_string = (const char *)glGetString( GL_RENDERER );
	glConfig.version_string = (const char *)glGetString( GL_VERSION );

	// OpenGL driver constants
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize );
	glGetIntegerv( GL_MAX_SAMPLES, &glConfig.maxSamples );

	// stubbed or broken drivers may have reported 0...
	if (glConfig.maxTextureSize <= 0) {
		glConfig.maxTextureSize = 256;
	}

	glConfig.isInitialized = true;

	// recheck all the extensions
	R_CheckPortableExtensions();
	GL_CheckErrors(true);

	GfxInfo_f( idCmdArgs() );

	if (r_glDebugOutput.GetInteger() == 1 || r_glDebugOutput.GetInteger() == 2) {
		common->Printf("OpenGL: debug output enabled ");
		if (r_glDebugOutput.GetInteger() == 1) {
			common->Printf("(async)\n");
			glEnable( GL_DEBUG_OUTPUT );
		}
		else {
			common->Printf("(sync)\n");
			glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
		}

		glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE );
		glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE );
		glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, GL_FALSE );
		glDebugMessageCallback( R_GLDebugOutput, NULL );
	}
	else {
		common->Printf("OpenGL: debug output disabled\n");
	}
	GL_CheckErrors(true);

	if (r_glCoreProfile.GetBool()) {
		common->Printf("OpenGL: core profile enabled, creating global vao\n");
		GLuint VaoID;
		glGenVertexArrays( 1, &VaoID );
		glBindVertexArray( VaoID );
	}
	else {
		common->Printf("OpenGL: core profile disabled\n");
	}
	GL_CheckErrors(true);

	// allocate the vertex array range or vertex objects
	vertexCache.Init();
	GL_CheckErrors(true);
	common->Printf("OpenGL: vertex cache initialized\n");

	cmdSystem->AddCommand( "reloadGlslPrograms", R_ReloadGlslPrograms_f, CMD_FL_RENDERER, "reloads GLSL programs" );

	fhRenderProgram::Init();
	R_ReloadGlslPrograms_f(idCmdArgs());
	GL_CheckErrors(true);
	common->Printf("OpenGL: render programs initialized\n");

	fhImmediateMode::Init();
	GL_CheckErrors(true);
	common->Printf("OpenGL: immediate mode initialized\n");

	// allocate the frame data, which may be more if smp is enabled
	R_InitFrameData();
	GL_CheckErrors(true);
	common->Printf("OpenGL: frame data initialized\n");

#ifdef _WIN32
	static bool glCheck = false;
	if (!glCheck && win32.osversion.dwMajorVersion == 6) {
		glCheck = true;
		if (!idStr::Icmp( glConfig.vendor_string, "Microsoft" ) && idStr::FindText( glConfig.renderer_string, "OpenGL-D3D" ) != -1) {
			if (cvarSystem->GetCVarBool( "r_fullscreen" )) {
				cmdSystem->BufferCommandText( CMD_EXEC_NOW, "vid_restart partial windowed\n" );
				Sys_GrabMouseCursor( false );
			}
			int ret = MessageBoxA( NULL, "Please install OpenGL drivers from your graphics hardware vendor to run " GAME_NAME ".\nYour OpenGL functionality is limited.",
				"Insufficient OpenGL capabilities", MB_OKCANCEL | MB_ICONWARNING | MB_TASKMODAL );
			if (ret == IDCANCEL) {
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
				cmdSystem->ExecuteCommandBuffer();
			}
			if (cvarSystem->GetCVarBool( "r_fullscreen" )) {
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "vid_restart\n" );
			}
		}
	}
#endif
	glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
	glEnable( GL_MULTISAMPLE );
	GL_CheckErrors(true);
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors_helper(bool force, const char* file, int line) {
    int		err;
    char	s[64];
	int		i;

	RB_PrintDebugOutput();

	// check for up to 10 errors pending
	for ( i = 0 ; i < 10 ; i++ ) {
		err = glGetError();
		if ( err == GL_NO_ERROR ) {
			return;
		}
		switch( err ) {
			case GL_INVALID_ENUM:
				strcpy( s, "GL_INVALID_ENUM" );
				break;
			case GL_INVALID_VALUE:
				strcpy( s, "GL_INVALID_VALUE" );
				break;
			case GL_INVALID_OPERATION:
				strcpy( s, "GL_INVALID_OPERATION" );
				break;
			case GL_STACK_OVERFLOW:
				strcpy( s, "GL_STACK_OVERFLOW" );
				break;
			case GL_STACK_UNDERFLOW:
				strcpy( s, "GL_STACK_UNDERFLOW" );
				break;
			case GL_OUT_OF_MEMORY:
				strcpy( s, "GL_OUT_OF_MEMORY" );
				break;
			default:
				idStr::snPrintf( s, sizeof(s), "%i", err);
				break;
		}

		if (force || !r_glDebugOutput.GetInteger() != 0) {
			common->Printf( "GL_CheckErrors: %s (%s:%d)\n", s, file, line);
		}
	}
}

/*
=====================
R_ReloadSurface_f

Reload the material displayed by r_showSurfaceInfo
=====================
*/
static void R_ReloadSurface_f( const idCmdArgs &args ) {
	modelTrace_t mt;
	idVec3 start, end;

	// start far enough away that we don't hit the player model
	start = tr.primaryView->renderView.vieworg + tr.primaryView->renderView.viewaxis[0] * 16;
	end = start + tr.primaryView->renderView.viewaxis[0] * 1000.0f;
	if ( !tr.primaryWorld->Trace( mt, start, end, 0.0f, false ) ) {
		return;
	}

	common->Printf( "Reloading %s\n", mt.material->GetName() );

	// reload the decl
	mt.material->base->Reload();

	// reload any images used by the decl
	mt.material->ReloadImages( false );
}



/*
==============
R_ListModes_f
==============
*/
static void R_ListModes_f( const idCmdArgs &args ) {
  static const char* ar[] = {
    "4:3",
    "16:9",
    "16:10"
  };

  common->Printf( "\n" );
  for (int i = 0; i < s_numVidModes; ++i) {
    common->Printf("  %i: %s (%s)\n", i, r_vidModes[i].description, ar[r_vidModes[i].aspectRatio]);
  }
  common->Printf( "\n" );
}




/*
=============
R_TestImage_f

Display the given image centered on the screen.
testimage <number>
testimage <filename>
=============
*/
void R_TestImage_f( const idCmdArgs &args ) {
	int imageNum;

	if ( tr.testVideo ) {
		delete tr.testVideo;
		tr.testVideo = NULL;
	}
	tr.testImage = NULL;

	if ( args.Argc() != 2 ) {
		return;
	}

	if ( idStr::IsNumeric( args.Argv(1) ) ) {
		imageNum = atoi( args.Argv(1) );
		if ( imageNum >= 0 && imageNum < globalImages->images.Num() ) {
			tr.testImage = globalImages->images[imageNum];
		}
	} else {
		tr.testImage = globalImages->ImageFromFile( args.Argv( 1 ), TF_DEFAULT, false, TR_REPEAT, TD_DEFAULT );
	}
}

/*
=============
R_TestVideo_f

Plays the cinematic file in a testImage
=============
*/
void R_TestVideo_f( const idCmdArgs &args ) {
	if ( tr.testVideo ) {
		delete tr.testVideo;
		tr.testVideo = NULL;
	}
	tr.testImage = NULL;

	if ( args.Argc() < 2 ) {
		return;
	}

	tr.testImage = globalImages->ImageFromFile( "_scratch", TF_DEFAULT, false, TR_REPEAT, TD_DEFAULT );
	tr.testVideo = idCinematic::Alloc();
	tr.testVideo->InitFromFile( args.Argv( 1 ), true );

	cinData_t	cin;
	cin = tr.testVideo->ImageForTime( 0 );
	if ( !cin.image ) {
		delete tr.testVideo;
		tr.testVideo = NULL;
		tr.testImage = NULL;
		return;
	}

	common->Printf( "%i x %i images\n", cin.imageWidth, cin.imageHeight );

	int	len = tr.testVideo->AnimationLength();
	common->Printf( "%5.1f seconds of video\n", len * 0.001 );

	tr.testVideoStartTime = tr.primaryRenderView.time * 0.001;

	// try to play the matching wav file
	idStr	wavString = args.Argv( ( args.Argc() == 2 ) ? 1 : 2 );
	wavString.StripFileExtension();
	wavString = wavString + ".wav";
	session->sw->PlayShaderDirectly( wavString.c_str() );
}

static int R_QsortSurfaceAreas( const void *a, const void *b ) {
	const idMaterial	*ea, *eb;
	int	ac, bc;

	ea = *(idMaterial **)a;
	if ( !ea->EverReferenced() ) {
		ac = 0;
	} else {
		ac = ea->GetSurfaceArea();
	}
	eb = *(idMaterial **)b;
	if ( !eb->EverReferenced() ) {
		bc = 0;
	} else {
		bc = eb->GetSurfaceArea();
	}

	if ( ac < bc ) {
		return -1;
	}
	if ( ac > bc ) {
		return 1;
	}

	return idStr::Icmp( ea->GetName(), eb->GetName() );
}


/*
===================
R_ReportSurfaceAreas_f

Prints a list of the materials sorted by surface area
===================
*/
void R_ReportSurfaceAreas_f( const idCmdArgs &args ) {
	int		i, count;
	idMaterial	**list;

	count = declManager->GetNumDecls( DECL_MATERIAL );
	list = (idMaterial **)_alloca( count * sizeof( *list ) );

	for ( i = 0 ; i < count ; i++ ) {
		list[i] = (idMaterial *)declManager->DeclByIndex( DECL_MATERIAL, i, false );
	}

	qsort( list, count, sizeof( list[0] ), R_QsortSurfaceAreas );

	// skip over ones with 0 area
	for ( i = 0 ; i < count ; i++ ) {
		if ( list[i]->GetSurfaceArea() > 0 ) {
			break;
		}
	}

	for ( ; i < count ; i++ ) {
		// report size in "editor blocks"
		int	blocks = list[i]->GetSurfaceArea() / 4096.0;
		common->Printf( "%7i %s\n", blocks, list[i]->GetName() );
	}
}

/*
===================
R_ReportImageDuplication_f

Checks for images with the same hash value and does a better comparison
===================
*/
void R_ReportImageDuplication_f( const idCmdArgs &args ) {
	int		i, j;

	common->Printf( "Images with duplicated contents:\n" );

	int	count = 0;

	for ( i = 0 ; i < globalImages->images.Num() ; i++ ) {
		idImage	*image1 = globalImages->images[i];

		if ( image1->isPartialImage ) {
			// ignore background loading stubs
			continue;
		}
		if ( image1->generatorFunction ) {
			// ignore procedural images
			continue;
		}
		if ( image1->cubeFiles != CF_2D ) {
			// ignore cube maps
			continue;
		}
		if ( image1->defaulted ) {
			continue;
		}

		fhImageData imageData;
		if (!imageData.LoadProgram( image1->imgName )) {
			continue;
		}

		for ( j = 0 ; j < i ; j++ ) {
			idImage	*image2 = globalImages->images[j];

			if ( image2->isPartialImage ) {
				continue;
			}
			if ( image2->generatorFunction ) {
				continue;
			}
			if ( image2->type != image1->type ) {
				continue;
			}
			if ( image2->defaulted ) {
				continue;
			}
			if ( image1->imageHash != image2->imageHash ) {
				continue;
			}
			if ( image2->uploadWidth != image1->uploadWidth
				|| image2->uploadHeight != image1->uploadHeight ) {
				continue;
			}
			if ( !idStr::Icmp( image1->imgName, image2->imgName ) ) {
				// ignore same image-with-different-parms
				continue;
			}


			fhImageData imageData2;
			if (!imageData2.LoadProgram( image2->imgName )) {
				continue;
			}

			if (imageData2.GetWidth() != imageData.GetWidth() || imageData2.GetHeight() != imageData.GetHeight()) {
				continue;
			}

			if (imageData2.GetPixelFormat() != imageData.GetPixelFormat() ) {
				continue;
			}

			if (imageData2.GetNumFaces() != imageData.GetNumFaces()) {
				continue;
			}

			if (imageData2.GetNumLevels() != imageData.GetNumLevels()) {
				continue;
			}

			if (imageData2.GetSize() != imageData.GetSize()) {
				continue;
			}

			//TODO(johl): we compare only the first level. Should we compare all levels?
			bool equal = true;;
			for (uint32 f = 0; f < imageData.GetNumFaces(); ++f) {
				if (memcmp( imageData2.GetData( f ), imageData.GetData( f ), imageData.GetSize() ) != 0) {
					equal = false;
					break;
				}
			}

			if (equal) {
				common->Printf( "%s == %s\n", image1->imgName.c_str(), image2->imgName.c_str() );
				session->UpdateScreen( true );
				count++;
				break;
			}
		}
	}
	common->Printf( "%i / %i collisions\n", count, globalImages->images.Num() );
}

/*
==============================================================================

						THROUGHPUT BENCHMARKING

==============================================================================
*/

/*
================
R_RenderingFPS
================
*/
static float R_RenderingFPS( const renderView_t *renderView ) {
	glFinish();

	int		start = Sys_Milliseconds();
	static const int SAMPLE_MSEC = 1000;
	int		end;
	int		count = 0;

	while( 1 ) {
		// render
		renderSystem->BeginFrame( glConfig.vidWidth, glConfig.vidHeight );
		tr.primaryWorld->RenderScene( renderView );
		renderSystem->EndFrame();
		glFinish();
		count++;
		end = Sys_Milliseconds();
		if ( end - start > SAMPLE_MSEC ) {
			break;
		}
	}

	float fps = count * 1000.0 / ( end - start );

	return fps;
}

/*
================
R_Benchmark_f
================
*/
void R_Benchmark_f( const idCmdArgs &args ) {
	float	fps, msec;
	renderView_t	view;

	if ( !tr.primaryView ) {
		common->Printf( "No primaryView for benchmarking\n" );
		return;
	}
	view = tr.primaryRenderView;

	for ( int size = 100 ; size >= 10 ; size -= 10 ) {
		r_screenFraction.SetInteger( size );
		fps = R_RenderingFPS( &view );
		int	kpix = glConfig.vidWidth * glConfig.vidHeight * ( size * 0.01 ) * ( size * 0.01 ) * 0.001;
		msec = 1000.0 / fps;
		common->Printf( "kpix: %4i  msec:%5.1f fps:%5.1f\n", kpix, msec, fps );
	}

	// enable r_singleTriangle 1 while r_screenFraction is still at 10
	r_singleTriangle.SetBool( 1 );
	fps = R_RenderingFPS( &view );
	msec = 1000.0 / fps;
	common->Printf( "single tri  msec:%5.1f fps:%5.1f\n", msec, fps );
	r_singleTriangle.SetBool( 0 );
	r_screenFraction.SetInteger( 100 );

	// enable r_skipRenderContext 1
	r_skipRenderContext.SetBool( true );
	fps = R_RenderingFPS( &view );
	msec = 1000.0 / fps;
	common->Printf( "no context  msec:%5.1f fps:%5.1f\n", msec, fps );
	r_skipRenderContext.SetBool( false );
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
====================
R_ReadPixels

Allows the rendering of an image larger than the actual window by
rendering into a resized off-screen framebuffer.

If ref isn't specified, the full session UpdateScreen will be done.
====================
*/
static void R_ReadPixels( int width, int height, byte *buffer, renderView_t *ref = nullptr ) {
	// include extra space for OpenGL padding to word boundaries

	const int tempSize = (width + 3) * height * 3;
	auto temp = (byte *)R_StaticAlloc(tempSize);

	const int oldWidth = glConfig.vidWidth;
	const int oldHeight = glConfig.vidHeight;

	fhFramebuffer* src = r_useFramebuffer.GetBool() ? fhFramebuffer::currentRenderFramebuffer2 : fhFramebuffer::defaultFramebuffer;

	if (ref) {
		tr.BeginFrame(width, height);
		tr.primaryWorld->RenderScene(ref);
		src = tr.LocalEndFrame().framebuffer;
	}
	else {
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
		session->UpdateScreen();
	}

	if (src && src != fhFramebuffer::defaultFramebuffer) {
		auto dst = fhFramebuffer::currentRenderFramebuffer;

		glViewport(0, 0, dst->GetWidth(), dst->GetHeight());
		glScissor(0, 0, dst->GetWidth(), dst->GetHeight());
		fhFramebuffer::BlitColor(src, dst);

		if (glConfig.extDirectStateAccessAvailable) {
			glGetTextureImageEXT(globalImages->currentRenderImage->texnum, GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, temp);
		}
		else {
			assert(glConfig.arbDirectStateAccessAvailable);
			glGetTextureImage(globalImages->currentRenderImage->texnum, 0, GL_RGB, GL_UNSIGNED_BYTE, tempSize, temp);
		}
	}
	else {
		//FIXME(johl): remove this, src should never be null
		common->Warning("reading pixel data from default framebuffer is deprecated!");
		glReadBuffer( GL_FRONT );
		glReadPixels( 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, temp );
	}

	const int row = (width * 3 + 3) & ~3;		// OpenGL pads to dword boundaries

	for (int y = 0; y < height; y++) {
		memcpy( buffer + (y* width) * 3, temp + y * row, width * 3 );
	}

	R_StaticFree( temp );

	glConfig.vidWidth = oldWidth;
	glConfig.vidHeight = oldHeight;
}

/*
==================
TakeScreenshot

Move to tr_imagefiles.c...

Will automatically tile render large screen shots if necessary
Downsample is the number of steps to mipmap the image before saving it
If ref == NULL, session->updateScreen will be used
==================
*/
void idRenderSystemLocal::TakeScreenshot( int width, int height, const char *fileName, int blends, renderView_t *ref ) {
	takingScreenshot = true;

	const int pix = width * height;

	byte* buffer = (byte *)R_StaticAlloc(pix*3 + 18);
	memset (buffer, 0, 18);

	if ( blends <= 1 ) {
		R_ReadPixels(width, height, buffer + 18, ref);
	} else {
		unsigned short *shortBuffer = (unsigned short *)R_StaticAlloc(pix*2*3);
		memset (shortBuffer, 0, pix*2*3);

		// enable anti-aliasing jitter
		r_jitter.SetBool( true );

		for ( int i = 0 ; i < blends ; i++ ) {
			R_ReadPixels(width, height, buffer + 18, ref);

			for ( int j = 0 ; j < pix*3 ; j++ ) {
				shortBuffer[j] += buffer[18+j];
			}
		}

		// divide back to bytes
		for ( int i = 0 ; i < pix*3 ; i++ ) {
			buffer[18+i] = shortBuffer[i] / blends;
		}

		R_StaticFree( shortBuffer );
		r_jitter.SetBool( false );
	}

	// fill in the header (this is vertically flipped, which glReadPixels emits)
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr
	const int c = 18 + width * height * 3;
	for (int i=18 ; i<c ; i+=3) {
		int temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

	// _D3XP adds viewnote screenie save to cdpath
	if ( strstr( fileName, "viewnote" ) ) {
		fileSystem->WriteFile( fileName, buffer, c, "fs_cdpath" );
	} else {
		fileSystem->WriteFile( fileName, buffer, c );
	}

	R_StaticFree( buffer );

	takingScreenshot = false;
}


/*
==================
R_ScreenshotFilename

Returns a filename with digits appended
if we have saved a previous screenshot, don't scan
from the beginning, because recording demo avis can involve
thousands of shots
==================
*/
void R_ScreenshotFilename( int &lastNumber, const char *base, idStr &fileName ) {
	int	a,b,c,d, e;

	bool restrict = cvarSystem->GetCVarBool( "fs_restrict" );
	cvarSystem->SetCVarBool( "fs_restrict", false );

	lastNumber++;
	if ( lastNumber > 99999 ) {
		lastNumber = 99999;
	}
	for ( ; lastNumber < 99999 ; lastNumber++ ) {
		int	frac = lastNumber;

		a = frac / 10000;
		frac -= a*10000;
		b = frac / 1000;
		frac -= b*1000;
		c = frac / 100;
		frac -= c*100;
		d = frac / 10;
		frac -= d*10;
		e = frac;

		sprintf( fileName, "%s%i%i%i%i%i.tga", base, a, b, c, d, e );
		if ( lastNumber == 99999 ) {
			break;
		}
		int len = fileSystem->ReadFile( fileName, NULL, NULL );
		if ( len <= 0 ) {
			break;
		}
		// check again...
	}
	cvarSystem->SetCVarBool( "fs_restrict", restrict );
}

/*
==================
R_BlendedScreenShot

screenshot
screenshot [filename]
screenshot [width] [height]
screenshot [width] [height] [samples]
==================
*/
void R_ScreenShot_f( const idCmdArgs &args ) {
	static const int maxBlends = 256; // to keep the accumulation in shorts
	static int lastNumber = 0;
	idStr checkname;

	int width = tr.GetScreenWidth();
	int height = tr.GetScreenHeight();
	int	blends = 1;

	switch ( args.Argc() ) {
	case 1:
		R_ScreenshotFilename( lastNumber, "screenshots/shot", checkname );
		break;
	case 2:
		checkname = args.Argv( 1 );
		break;
	case 3:
		width = atoi( args.Argv( 1 ) );
		height = atoi( args.Argv( 2 ) );
		R_ScreenshotFilename( lastNumber, "screenshots/shot", checkname );
		break;
	case 4:
		width = atoi( args.Argv( 1 ) );
		height = atoi( args.Argv( 2 ) );
		blends = atoi( args.Argv( 3 ) );
		if ( blends < 1 ) {
			blends = 1;
		}

		if (blends > maxBlends) {
			blends = maxBlends;
		}
		R_ScreenshotFilename( lastNumber, "screenshots/shot", checkname );
		break;
	default:
		common->Printf( "usage: screenshot\n       screenshot <filename>\n       screenshot <width> <height>\n       screenshot <width> <height> <blends>\n" );
		return;
	}

	// put the console away
	console->Close();

	tr.TakeScreenshot( width, height, checkname, blends, NULL );

	common->Printf( "Wrote %s\n", checkname.c_str() );
}

/*
===============
R_StencilShot
Save out a screenshot showing the stencil buffer expanded by 16x range
===============
*/
void R_StencilShot( void ) {
	byte		*buffer;
	int			i, c;

	int	width = tr.GetScreenWidth();
	int	height = tr.GetScreenHeight();

	int	pix = width * height;

	c = pix * 3 + 18;
	buffer = (byte *)Mem_Alloc(c);
	memset (buffer, 0, 18);

	byte *byteBuffer = (byte *)Mem_Alloc(pix);

	glReadPixels( 0, 0, width, height, GL_STENCIL_INDEX , GL_UNSIGNED_BYTE, byteBuffer );

	for ( i = 0 ; i < pix ; i++ ) {
		buffer[18+i*3] =
		buffer[18+i*3+1] =
			//		buffer[18+i*3+2] = ( byteBuffer[i] & 15 ) * 16;
		buffer[18+i*3+2] = byteBuffer[i];
	}

	// fill in the header (this is vertically flipped, which glReadPixels emits)
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	fileSystem->WriteFile( "screenshots/stencilShot.tga", buffer, c, "fs_savepath" );

	Mem_Free( buffer );
	Mem_Free( byteBuffer );
}

/*
==================
R_EnvShot_f

envshot <basename>

Saves out env/<basename>_ft.tga, etc
==================
*/
void R_EnvShot_f( const idCmdArgs &args ) {
	const char	*extensions[6] =  { "_px.tga", "_nx.tga", "_py.tga", "_ny.tga",
		"_pz.tga", "_nz.tga" };

	if ( args.Argc() != 2 && args.Argc() != 3 && args.Argc() != 4 ) {
		common->Printf( "USAGE: envshot <basename> [size] [blends]\n" );
		return;
	}

	int blends = 1;
	int size = 256;
	if ( args.Argc() == 4 ) {
		size = atoi( args.Argv( 2 ) );
		blends = atoi( args.Argv( 3 ) );
	} else if ( args.Argc() == 3 ) {
		size = atoi( args.Argv( 2 ) );
		blends = 1;
	}

	idMat3 axis[6];
	memset( &axis, 0, sizeof( axis ) );
	axis[0][0][0] = 1;
	axis[0][1][2] = 1;
	axis[0][2][1] = 1;

	axis[1][0][0] = -1;
	axis[1][1][2] = -1;
	axis[1][2][1] = 1;

	axis[2][0][1] = 1;
	axis[2][1][0] = -1;
	axis[2][2][2] = -1;

	axis[3][0][1] = -1;
	axis[3][1][0] = -1;
	axis[3][2][2] = 1;

	axis[4][0][2] = 1;
	axis[4][1][0] = -1;
	axis[4][2][1] = 1;

	axis[5][0][2] = -1;
	axis[5][1][0] = 1;
	axis[5][2][1] = 1;

	if (!tr.primaryView) {
		common->Printf("No primary view.\n");
		return;
	}

	const char* baseName = args.Argv(1);

	for ( int i = 0 ; i < 6 ; i++ ) {
		renderView_t ref = tr.primaryView->renderView;
		ref.x = ref.y = 0;
		ref.fov_x = ref.fov_y = 90;
		ref.viewaxis = axis[i];
		idStr		fullname;
		sprintf( fullname, "env/%s%s", baseName, extensions[i] );
		tr.TakeScreenshot( size, size, fullname, blends, &ref );
		common->Printf("Wrote %s, etc\n", fullname.c_str());
	}
}

//============================================================================

static idMat3		cubeAxis[6];


/*
==================
R_SampleCubeMap
==================
*/
static void R_SampleCubeMap( const idVec3 &dir, int size, byte *buffers[6], byte result[4] ) {
	float	adir[3];
	int		axis, x, y;

	adir[0] = fabs(dir[0]);
	adir[1] = fabs(dir[1]);
	adir[2] = fabs(dir[2]);

	if ( dir[0] >= adir[1] && dir[0] >= adir[2] ) {
		axis = 0;
	} else if ( -dir[0] >= adir[1] && -dir[0] >= adir[2] ) {
		axis = 1;
	} else if ( dir[1] >= adir[0] && dir[1] >= adir[2] ) {
		axis = 2;
	} else if ( -dir[1] >= adir[0] && -dir[1] >= adir[2] ) {
		axis = 3;
	} else if ( dir[2] >= adir[1] && dir[2] >= adir[2] ) {
		axis = 4;
	} else {
		axis = 5;
	}

	float	fx = (dir * cubeAxis[axis][1]) / (dir * cubeAxis[axis][0]);
	float	fy = (dir * cubeAxis[axis][2]) / (dir * cubeAxis[axis][0]);

	fx = -fx;
	fy = -fy;
	x = size * 0.5 * (fx + 1);
	y = size * 0.5 * (fy + 1);
	if ( x < 0 ) {
		x = 0;
	} else if ( x >= size ) {
		x = size-1;
	}
	if ( y < 0 ) {
		y = 0;
	} else if ( y >= size ) {
		y = size-1;
	}

	result[0] = buffers[axis][(y*size+x)*4+0];
	result[1] = buffers[axis][(y*size+x)*4+1];
	result[2] = buffers[axis][(y*size+x)*4+2];
	result[3] = buffers[axis][(y*size+x)*4+3];
}

/*
==================
R_MakeAmbientMap_f

R_MakeAmbientMap_f <basename> [size]

Saves out env/<basename>_amb_ft.tga, etc
==================
*/
void R_MakeAmbientMap_f( const idCmdArgs &args ) {
	idStr fullname;
	const char	*baseName;
	renderView_t	ref;
	viewDef_t	primary;
	int			downSample;
	const char	*extensions[6] =  { "_px.tga", "_nx.tga", "_py.tga", "_ny.tga",
		"_pz.tga", "_nz.tga" };
	int			outSize;
	byte		*buffers[6];
	int			width, height;

	if ( args.Argc() != 2 && args.Argc() != 3 ) {
		common->Printf( "USAGE: ambientshot <basename> [size]\n" );
		return;
	}
	baseName = args.Argv( 1 );

	downSample = 0;
	if ( args.Argc() == 3 ) {
		outSize = atoi( args.Argv( 2 ) );
	} else {
		outSize = 32;
	}

	memset( &cubeAxis, 0, sizeof( cubeAxis ) );
	cubeAxis[0][0][0] = 1;
	cubeAxis[0][1][2] = 1;
	cubeAxis[0][2][1] = 1;

	cubeAxis[1][0][0] = -1;
	cubeAxis[1][1][2] = -1;
	cubeAxis[1][2][1] = 1;

	cubeAxis[2][0][1] = 1;
	cubeAxis[2][1][0] = -1;
	cubeAxis[2][2][2] = -1;

	cubeAxis[3][0][1] = -1;
	cubeAxis[3][1][0] = -1;
	cubeAxis[3][2][2] = 1;

	cubeAxis[4][0][2] = 1;
	cubeAxis[4][1][0] = -1;
	cubeAxis[4][2][1] = 1;

	cubeAxis[5][0][2] = -1;
	cubeAxis[5][1][0] = 1;
	cubeAxis[5][2][1] = 1;

	// read all of the images
	for ( int i = 0 ; i < 6 ; i++ ) {
		sprintf( fullname, "env/%s%s", baseName, extensions[i] );
		common->Printf( "loading %s\n", fullname.c_str() );
		session->UpdateScreen();
		R_LoadImage( fullname, &buffers[i], &width, &height, NULL, true );
		if ( !buffers[i] ) {
			common->Printf( "failed.\n" );
			for ( i-- ; i >= 0 ; i-- ) {
				Mem_Free( buffers[i] );
			}
			return;
		}
	}

	// resample with hemispherical blending
	const int	samples = 1000;

	byte	*outBuffer = (byte *)_alloca( outSize * outSize * 4 );

	for ( int map = 0 ; map < 2 ; map++ ) {
		for ( int i = 0 ; i < 6 ; i++ ) {
			for ( int x = 0 ; x < outSize ; x++ ) {
				for ( int y = 0 ; y < outSize ; y++ ) {
					idVec3	dir;
					float	total[3];

					dir = cubeAxis[i][0] + -( -1 + 2.0*x/(outSize-1) ) * cubeAxis[i][1] + -( -1 + 2.0*y/(outSize-1) ) * cubeAxis[i][2];
					dir.Normalize();
					total[0] = total[1] = total[2] = 0;

					float	limit = map ? 0.95 : 0.25;		// small for specular, almost hemisphere for ambient

					for ( int s = 0 ; s < samples ; s++ ) {
						// pick a random direction vector that is inside the unit sphere but not behind dir,
						// which is a robust way to evenly sample a hemisphere
						idVec3	test;
						while( 1 ) {
							for ( int j = 0 ; j < 3 ; j++ ) {
								test[j] = -1 + 2 * (rand()&0x7fff)/(float)0x7fff;
							}
							if ( test.Length() > 1.0 ) {
								continue;
							}
							test.Normalize();
							if ( test * dir > limit ) {	// don't do a complete hemisphere
								break;
							}
						}
						byte	result[4];
	//test = dir;
						R_SampleCubeMap( test, width, buffers, result );
						total[0] += result[0];
						total[1] += result[1];
						total[2] += result[2];
					}
					outBuffer[(y*outSize+x)*4+0] = total[0] / samples;
					outBuffer[(y*outSize+x)*4+1] = total[1] / samples;
					outBuffer[(y*outSize+x)*4+2] = total[2] / samples;
					outBuffer[(y*outSize+x)*4+3] = 255;
				}
			}

			if ( map == 0 ) {
				sprintf( fullname, "env/%s_amb%s", baseName, extensions[i] );
			} else {
				sprintf( fullname, "env/%s_spec%s", baseName, extensions[i] );
			}
			common->Printf( "writing %s\n", fullname.c_str() );
			session->UpdateScreen();
			R_WriteTGA( fullname, outBuffer, outSize, outSize );
		}
	}

	for ( int i = 0 ; i < 6 ; i++ ) {
		if ( buffers[i] ) {
			Mem_Free( buffers[i] );
		}
	}
}

//============================================================================

/*
================
GfxInfo_f
================
*/
void GfxInfo_f( const idCmdArgs &args ) {
	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};

	common->Printf( "\nGL_VENDOR: %s\n", glConfig.vendor_string );
	common->Printf( "GL_RENDERER: %s\n", glConfig.renderer_string );
	common->Printf( "GL_VERSION: %s\n", glConfig.version_string );
	common->Printf( "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	common->Printf( "GL_MAX_TEXTURE_IMAGE_UNITS_ARB: %d\n", glConfig.maxTextureImageUnits );
	common->Printf( "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
	common->Printf( "MODE: %d, %d x %d %s hz:", r_mode.GetInteger(), glConfig.vidWidth, glConfig.vidHeight, fsstrings[r_fullscreen.GetBool()] );
	common->Printf( "CPU: %s\n", Sys_GetProcessorString() );

	//=============================

	common->Printf( "-------\n" );

	if ( r_finish.GetBool() ) {
		common->Printf( "Forcing glFinish\n" );
	} else {
		common->Printf( "glFinish not forced\n" );
	}

#ifdef _WIN32
// WGL_EXT_swap_interval
typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
extern	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

	if ( r_swapInterval.GetInteger() && wglSwapIntervalEXT ) {
		common->Printf( "Forcing swapInterval %i\n", r_swapInterval.GetInteger() );
	} else {
		common->Printf( "swapInterval not forced\n" );
	}
#endif
}

/*
=================
R_VidRestart_f
=================
*/
void R_VidRestart_f( const idCmdArgs &args ) {
	// if OpenGL isn't started, do nothing
	if ( !glConfig.isInitialized ) {
		return;
	}

	bool full = true;
	bool forceWindow = false;
	for ( int i = 1 ; i < args.Argc() ; i++ ) {
		if ( idStr::Icmp( args.Argv( i ), "partial" ) == 0 ) {
			full = false;
			continue;
		}
		if ( idStr::Icmp( args.Argv( i ), "windowed" ) == 0 ) {
			forceWindow = true;
			continue;
		}
	}

	// this could take a while, so give them the cursor back ASAP
	Sys_GrabMouseCursor( false );

	// dump ambient caches
	renderModelManager->FreeModelVertexCaches();

	// free any current world interaction surfaces and vertex caches
	R_FreeDerivedData();

	// make sure the defered frees are actually freed
	R_ToggleSmpFrame();
	R_ToggleSmpFrame();

	// free the vertex caches so they will be regenerated again
	vertexCache.PurgeAll();

	// sound and input are tied to the window we are about to destroy

	if ( full ) {
		// free all of our texture numbers
		soundSystem->ShutdownHW();
		Sys_ShutdownInput();
		fhFramebuffer::PurgeAll();
		globalImages->PurgeAllImages();
		fhSampler::PurgeAll();
		fhRenderProgram::PurgeAll();
		// free the context and close the window
		GLimp_Shutdown();
		glConfig.isInitialized = false;

		// create the new context and vertex cache
		bool latch = cvarSystem->GetCVarBool( "r_fullscreen" );
		if ( forceWindow ) {
			cvarSystem->SetCVarBool( "r_fullscreen", false );
		}
		R_InitOpenGL();
		cvarSystem->SetCVarBool( "r_fullscreen", latch );

		// regenerate all images
		globalImages->ReloadAllImages();
		fhRenderProgram::ReloadAll();
	} else {
		glimpParms_t	parms;
		parms.width = glConfig.vidWidth;
		parms.height = glConfig.vidHeight;
		parms.fullScreen = ( forceWindow ) ? false : r_fullscreen.GetBool();
		parms.displayHz = r_displayRefresh.GetInteger();
		parms.multiSamples = r_multiSamples.GetInteger();
		GLimp_SetScreenParms( parms );
	}



	// make sure the regeneration doesn't use anything no longer valid
	tr.viewCount++;
	tr.viewDef = NULL;

	// regenerate all necessary interactions
	R_RegenerateWorld_f( idCmdArgs() );

	// check for problems
	GL_CheckErrors(true);

	// start sound playing again
	soundSystem->SetMute( false );
}


/*
=================
R_InitMaterials
=================
*/
void R_InitMaterials( void ) {
	tr.defaultMaterial = declManager->FindMaterial( "_default", false );
	if ( !tr.defaultMaterial ) {
		common->FatalError( "_default material not found" );
	}
	declManager->FindMaterial( "_default", false );

	// needed by R_DeriveLightData
	declManager->FindMaterial( "lights/defaultPointLight" );
	declManager->FindMaterial( "lights/defaultProjectedLight" );
}


/*
=================
R_SizeUp_f

Keybinding command
=================
*/
static void R_SizeUp_f( const idCmdArgs &args ) {
	if ( r_screenFraction.GetInteger() + 10 > 100 ) {
		r_screenFraction.SetInteger( 100 );
	} else {
		r_screenFraction.SetInteger( r_screenFraction.GetInteger() + 10 );
	}
}


/*
=================
R_SizeDown_f

Keybinding command
=================
*/
static void R_SizeDown_f( const idCmdArgs &args ) {
	if ( r_screenFraction.GetInteger() - 10 < 10 ) {
		r_screenFraction.SetInteger( 10 );
	} else {
		r_screenFraction.SetInteger( r_screenFraction.GetInteger() - 10 );
	}
}

/*
===============
TouchGui_f

  this is called from the main thread
===============
*/
void R_TouchGui_f( const idCmdArgs &args ) {
	const char	*gui = args.Argv( 1 );

	if ( !gui[0] ) {
		common->Printf( "USAGE: touchGui <guiName>\n" );
		return;
	}

	common->Printf( "touchGui %s\n", gui );
	session->UpdateScreen();
	uiManager->Touch( gui );
}

/*
=================
R_InitCommands
=================
*/
void R_InitCommands( void ) {
	cmdSystem->AddCommand( "MakeMegaTexture", idMegaTexture::MakeMegaTexture_f, CMD_FL_RENDERER|CMD_FL_CHEAT, "processes giant images" );
	cmdSystem->AddCommand( "sizeUp", R_SizeUp_f, CMD_FL_RENDERER, "makes the rendered view larger" );
	cmdSystem->AddCommand( "sizeDown", R_SizeDown_f, CMD_FL_RENDERER, "makes the rendered view smaller" );
	cmdSystem->AddCommand( "reloadGuis", R_ReloadGuis_f, CMD_FL_RENDERER, "reloads guis" );
	cmdSystem->AddCommand( "listGuis", R_ListGuis_f, CMD_FL_RENDERER, "lists guis" );
	cmdSystem->AddCommand( "touchGui", R_TouchGui_f, CMD_FL_RENDERER, "touches a gui" );
	cmdSystem->AddCommand( "screenshot", R_ScreenShot_f, CMD_FL_RENDERER, "takes a screenshot" );
	cmdSystem->AddCommand( "envshot", R_EnvShot_f, CMD_FL_RENDERER, "takes an environment shot" );
	cmdSystem->AddCommand( "makeAmbientMap", R_MakeAmbientMap_f, CMD_FL_RENDERER|CMD_FL_CHEAT, "makes an ambient map" );
	cmdSystem->AddCommand( "benchmark", R_Benchmark_f, CMD_FL_RENDERER, "benchmark" );
	cmdSystem->AddCommand( "gfxInfo", GfxInfo_f, CMD_FL_RENDERER, "show graphics info" );
	cmdSystem->AddCommand( "modulateLights", R_ModulateLights_f, CMD_FL_RENDERER | CMD_FL_CHEAT, "modifies shader parms on all lights" );
	cmdSystem->AddCommand( "testImage", R_TestImage_f, CMD_FL_RENDERER | CMD_FL_CHEAT, "displays the given image centered on screen", idCmdSystem::ArgCompletion_ImageName );
	cmdSystem->AddCommand( "testVideo", R_TestVideo_f, CMD_FL_RENDERER | CMD_FL_CHEAT, "displays the given cinematic", idCmdSystem::ArgCompletion_VideoName );
	cmdSystem->AddCommand( "reportSurfaceAreas", R_ReportSurfaceAreas_f, CMD_FL_RENDERER, "lists all used materials sorted by surface area" );
	cmdSystem->AddCommand( "reportImageDuplication", R_ReportImageDuplication_f, CMD_FL_RENDERER, "checks all referenced images for duplications" );
	cmdSystem->AddCommand( "regenerateWorld", R_RegenerateWorld_f, CMD_FL_RENDERER, "regenerates all interactions" );
	cmdSystem->AddCommand( "showInteractionMemory", R_ShowInteractionMemory_f, CMD_FL_RENDERER, "shows memory used by interactions" );
	cmdSystem->AddCommand( "showTriSurfMemory", R_ShowTriSurfMemory_f, CMD_FL_RENDERER, "shows memory used by triangle surfaces" );
	cmdSystem->AddCommand( "vid_restart", R_VidRestart_f, CMD_FL_RENDERER, "restarts renderSystem" );
	cmdSystem->AddCommand( "listRenderEntityDefs", R_ListRenderEntityDefs_f, CMD_FL_RENDERER, "lists the entity defs" );
	cmdSystem->AddCommand( "listRenderLightDefs", R_ListRenderLightDefs_f, CMD_FL_RENDERER, "lists the light defs" );
	cmdSystem->AddCommand( "listModes", R_ListModes_f, CMD_FL_RENDERER, "lists all video modes" );
	cmdSystem->AddCommand( "reloadSurface", R_ReloadSurface_f, CMD_FL_RENDERER, "reloads the decl and images for selected surface" );
}

/*
===============
idRenderSystemLocal::Clear
===============
*/
void idRenderSystemLocal::Clear( void ) {
	frameCount = 0;
	viewCount = 0;
	staticAllocCount = 0;
	frameShaderTime = 0.0f;

	sortOffset = 0;
	worlds.Clear();
	primaryWorld = NULL;
	memset( &primaryRenderView, 0, sizeof( primaryRenderView ) );
	primaryView = NULL;
	defaultMaterial = NULL;
	testImage = NULL;
	viewDef = NULL;
	memset( &pc, 0, sizeof( pc ) );
	memset( &lockSurfacesCmd, 0, sizeof( lockSurfacesCmd ) );
	memset( &identitySpace, 0, sizeof( identitySpace ) );
	memset( renderCrops, 0, sizeof( renderCrops ) );
	currentRenderCrop = 0;
	guiRecursionLevel = 0;
	guiModel = NULL;
	demoGuiModel = NULL;
	takingScreenshot = false;
}

/*
===============
idRenderSystemLocal::Init
===============
*/
void idRenderSystemLocal::Init( void ) {

	common->Printf( "------- Initializing renderSystem --------\n" );

	// clear all our internal state
	viewCount = 1;		// so cleared structures never match viewCount
	// we used to memset tr, but now that it is a class, we can't, so
	// there may be other state we need to reset

	memset( &backEnd, 0, sizeof( backEnd ) );

	R_InitCommands();

	guiModel = new idGuiModel;
	guiModel->Clear();

	demoGuiModel = new idGuiModel;
	demoGuiModel->Clear();

	R_InitTriSurfData();

	globalImages->Init();
	fhFramebuffer::Init();

	idCinematic::InitCinematic( );

	R_InitMaterials();

	renderModelManager->Init();

	// set the identity space
	identitySpace.modelMatrix[0*4+0] = 1.0f;
	identitySpace.modelMatrix[1*4+1] = 1.0f;
	identitySpace.modelMatrix[2*4+2] = 1.0f;

	common->Printf( "renderSystem initialized.\n" );
	common->Printf( "--------------------------------------\n" );
}

/*
===============
idRenderSystemLocal::Shutdown
===============
*/
void idRenderSystemLocal::Shutdown( void ) {
	common->Printf( "idRenderSystem::Shutdown()\n" );

	R_DoneFreeType( );

	if ( glConfig.isInitialized ) {
		fhRenderProgram::PurgeAll();
		fhFramebuffer::PurgeAll();
		fhSampler::PurgeAll();
		globalImages->PurgeAllImages();
	}

	renderModelManager->Shutdown();

	idCinematic::ShutdownCinematic( );

	globalImages->Shutdown();

	// free frame memory
	R_ShutdownFrameData();

	// free the vertex cache, which should have nothing allocated now
	vertexCache.Shutdown();

	R_ShutdownTriSurfData();

	RB_ShutdownDebugTools();

	delete guiModel;
	delete demoGuiModel;

	Clear();

	ShutdownOpenGL();
}

/*
========================
idRenderSystemLocal::BeginLevelLoad
========================
*/
void idRenderSystemLocal::BeginLevelLoad( void ) {
	renderModelManager->BeginLevelLoad();
	globalImages->BeginLevelLoad();
}

/*
========================
idRenderSystemLocal::EndLevelLoad
========================
*/
void idRenderSystemLocal::EndLevelLoad( void ) {
	renderModelManager->EndLevelLoad();
	globalImages->EndLevelLoad();
	if ( r_forceLoadImages.GetBool() ) {
		RB_ShowImages();
	}
}

/*
========================
idRenderSystemLocal::InitOpenGL
========================
*/
void idRenderSystemLocal::InitOpenGL( void ) {
	// if OpenGL isn't started, start it now
	if ( !glConfig.isInitialized ) {
		R_InitOpenGL();
		globalImages->ReloadAllImages();
		GL_CheckErrors(true);
	}
}

/*
========================
idRenderSystemLocal::ShutdownOpenGL
========================
*/
void idRenderSystemLocal::ShutdownOpenGL( void ) {
	// free the context and close the window
	R_ShutdownFrameData();
	GLimp_Shutdown();
	glConfig.isInitialized = false;
}

/*
========================
idRenderSystemLocal::IsOpenGLRunning
========================
*/
bool idRenderSystemLocal::IsOpenGLRunning( void ) const {
	if ( !glConfig.isInitialized ) {
		return false;
	}
	return true;
}

/*
========================
idRenderSystemLocal::IsFullScreen
========================
*/
bool idRenderSystemLocal::IsFullScreen( void ) const {
	return glConfig.isFullscreen;
}

/*
========================
idRenderSystemLocal::GetScreenWidth
========================
*/
int idRenderSystemLocal::GetScreenWidth( void ) const {
	return glConfig.vidWidth;
}

/*
========================
idRenderSystemLocal::GetScreenHeight
========================
*/
int idRenderSystemLocal::GetScreenHeight( void ) const {
	return glConfig.vidHeight;
}

/*
========================
idRenderSystemLocal::GetScreenAspectRatio
========================
*/
int idRenderSystemLocal::GetScreenAspectRatio(void) const {
  return glConfig.vidAspectRatio;
}
