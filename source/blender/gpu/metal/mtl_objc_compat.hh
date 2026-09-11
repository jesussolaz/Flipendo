/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Puente de migracion del backend Metal de Objective-C++ a C++ puro.
 *
 * ANDAMIO TEMPORAL. Existe solo mientras queden ficheros `.mm` en este directorio.
 * Cuando el ultimo `.mm` de `gpu/metal` pase a `.cc`, la rama `__OBJC__` se borra y
 * las cabeceras se quedan con los tipos de metal-cpp a secas.
 *
 * EL PROBLEMA
 *
 * Las cabeceras de este backend declaran miembros con tipos Objective-C
 * (`id<MTLDevice>`, `MTLRenderPassDescriptor *`, `MTLPixelFormat`...). Un `.cc` no
 * puede ni leerlas, asi que ningun fichero de `gpu/metal` podia dejar de ser `.mm`.
 * Y no se pueden convertir de golpe: `mtl_context.hh` lo incluyen 22 ficheros, y
 * cambiarla romperia los 19 `.mm` restantes a la vez.
 *
 * LA SOLUCION: un nombre de tipo que valga en LOS DOS modos.
 *
 * Para cada protocolo Metal se declara un alias `XxxPtr`:
 *
 *     en un `.mm` :  using MTLDevicePtr = id<MTLDevice>;
 *     en un `.cc` :  using MTLDevicePtr = MTL::Device *;
 *
 * Las cabeceras escriben `MTLDevicePtr` y dejan de tener sintaxis de Objective-C.
 * Los `.mm` que aun no se han migrado NO cambian: para ellos el miembro sigue
 * siendo exactamente `id<MTLDevice>` y sus `[obj mensaje]` siguen compilando. Asi se
 * migra un `.mm` cada vez en lugar de los veinte de golpe.
 *
 * POR QUE ES SEGURO (leer antes de tocar nada)
 *
 * `id<MTLDevice>` y `MTL::Device *` son EL MISMO PUNTERO en ejecucion: los dos son
 * `objc_object *`. metal-cpp no envuelve ni copia nada; declara clases sin miembros
 * de datos cuyos metodos llaman a `objc_msgSend`. Una estructura que declare el
 * miembro de una forma u otra tiene el MISMO tamano y la MISMA disposicion, asi que
 * un `.mm` y un `.cc` pueden discrepar en la grafia sin romper ABI ni enlazado.
 *
 * TRAMPA MEDIDA: `id<T>` NO se puede imitar en C++.
 *
 * El primer intento fue declarar `template<typename T> using id = T *;` para que la
 * grafia `id<MTLDevice>` siguiera valiendo sin tocar las cabeceras. NO FUNCIONA:
 * metal-cpp incluye <objc/runtime.h>, que arrastra <objc/objc.h>, que ya hace
 * `typedef struct objc_object *id;`. El compilador da «redefinition of 'id' as a
 * different kind of symbol». Por eso hay que cambiar la grafia en las cabeceras y no
 * se puede resolver solo con alias.
 *
 * SEGUNDA TRAMPA: MTLBuffer y MTLTexture son nombres OCUPADOS.
 *
 * Blender ya tiene sus clases C++ `blender::gpu::MTLBuffer` (mtl_memory.hh) y
 * `blender::gpu::MTLTexture` (mtl_texture.hh). En Objective-C no chocan con los
 * protocolos Metal homonimos porque los protocolos tienen su propio espacio de
 * nombres; en C++ chocarian, y dentro de `namespace blender::gpu` ganaria la clase de
 * Blender. Un hipotetico `id<MTLBuffer>` traducido a ciegas habria dado
 * `blender::gpu::MTLBuffer *`: compila, y es SILENCIOSAMENTE el tipo equivocado.
 * `MTLBufferPtr` / `MTLTexturePtr` no son ambiguos en ningun modo, que es justo el
 * motivo de darles nombre propio.
 */

#pragma once

#ifdef __OBJC__

/* Traduccion Objective-C++: los tipos son los del SDK, como hasta ahora. */
#  include <Metal/Metal.h>
#  include <QuartzCore/QuartzCore.h>

using MTLDevicePtr = id<MTLDevice>;
using MTLCommandQueuePtr = id<MTLCommandQueue>;
using MTLCommandBufferPtr = id<MTLCommandBuffer>;
using MTLRenderCommandEncoderPtr = id<MTLRenderCommandEncoder>;
using MTLBlitCommandEncoderPtr = id<MTLBlitCommandEncoder>;
using MTLComputeCommandEncoderPtr = id<MTLComputeCommandEncoder>;
using MTLComputePipelineStatePtr = id<MTLComputePipelineState>;
using MTLRenderPipelineStatePtr = id<MTLRenderPipelineState>;
using MTLDepthStencilStatePtr = id<MTLDepthStencilState>;
using MTLSamplerStatePtr = id<MTLSamplerState>;
using MTLLibraryPtr = id<MTLLibrary>;
using MTLFunctionPtr = id<MTLFunction>;
using MTLArgumentEncoderPtr = id<MTLArgumentEncoder>;
using MTLEventPtr = id<MTLEvent>;
using MTLSharedEventPtr = id<MTLSharedEvent>;
using MTLCaptureScopePtr = id<MTLCaptureScope>;
using MTLFencePtr = id<MTLFence>;
using MTLHeapPtr = id<MTLHeap>;
using MTLBufferPtr = id<MTLBuffer>;
using MTLTexturePtr = id<MTLTexture>;
using MTLDrawablePtr = id<MTLDrawable>;
using CAMetalDrawablePtr = id<CAMetalDrawable>;
/* CAMetalLayer es una CLASE de QuartzCore, no un protocolo: se escribe
 * `CAMetalLayer *` y no `id<CAMetalLayer>`. Confundirlo rompia los 19 .mm
 * a la vez con «type arguments cannot be applied to non-class type 'id'». */
using CAMetalLayerPtr = CAMetalLayer *;

/* Literal de cadena vacia de Objective-C (ver la nota en la rama C++). */
#  define MTL_NSSTRING_EMPTY @""

#else

/* Traduccion C++ pura: los mismos tipos, via metal-cpp. */
#  include <Foundation/Foundation.hpp>
#  include <Metal/Metal.hpp>
#  include <QuartzCore/QuartzCore.hpp>

using MTLDevicePtr = MTL::Device *;
using MTLCommandQueuePtr = MTL::CommandQueue *;
using MTLCommandBufferPtr = MTL::CommandBuffer *;
using MTLRenderCommandEncoderPtr = MTL::RenderCommandEncoder *;
using MTLBlitCommandEncoderPtr = MTL::BlitCommandEncoder *;
using MTLComputeCommandEncoderPtr = MTL::ComputeCommandEncoder *;
using MTLComputePipelineStatePtr = MTL::ComputePipelineState *;
using MTLRenderPipelineStatePtr = MTL::RenderPipelineState *;
using MTLDepthStencilStatePtr = MTL::DepthStencilState *;
using MTLSamplerStatePtr = MTL::SamplerState *;
using MTLLibraryPtr = MTL::Library *;
using MTLFunctionPtr = MTL::Function *;
using MTLArgumentEncoderPtr = MTL::ArgumentEncoder *;
using MTLEventPtr = MTL::Event *;
using MTLSharedEventPtr = MTL::SharedEvent *;
using MTLCaptureScopePtr = MTL::CaptureScope *;
using MTLFencePtr = MTL::Fence *;
using MTLHeapPtr = MTL::Heap *;
using MTLBufferPtr = MTL::Buffer *;
using MTLTexturePtr = MTL::Texture *;
using MTLDrawablePtr = MTL::Drawable *;
using CAMetalDrawablePtr = CA::MetalDrawable *;
using CAMetalLayerPtr = CA::MetalLayer *;

/* En Objective-C `@""` es un literal inmortal creado por el compilador. En C++ puro
 * no hay literal equivalente, y fabricar un NS::String en el inicializador de un
 * miembro significaria reservar memoria en cada construccion. Se usa `nullptr`.
 *
 * NO ES EQUIVALENTE AL 100%%: en Objective-C mandar un mensaje a `nil` devuelve 0/nil
 * sin fallar, asi que `[s length]` da 0 tanto con `nil` como con `@""`, pero
 * `[array addObject:s]` si distingue. Hoy los unicos miembros afectados son los seis
 * `NSString *` de mtl_shader.hh y SOLO los toca mtl_shader.mm, que sigue siendo
 * Objective-C++ y por tanto sigue viendo `@""`. Ningun `.cc` construye todavia un
 * MTLShader. Cuando se migre mtl_shader.mm hay que resolver esto de verdad
 * (NS::String::string(...) o un std::string) y verificarlo, no darlo por hecho. */
#  define MTL_NSSTRING_EMPTY nullptr

/* Alias de los nombres Objective-C a los de metal-cpp. GENERADO mecanicamente
 * cruzando lo que declara `extern/metal-cpp/Metal/ *.hpp` con los nombres que usa
 * este backend; la correspondencia es siempre quitar el prefijo `MTL` y entrar en
 * el espacio de nombres `MTL::`.
 *
 * Se aliasa el enumerado COMPLETO y no solo lo que hoy aparece escrito, por una
 * razon medida: `mtl_shader.hh` fabrica nombres como `MTLVertexFormatCharNormalized`
 * pegando tokens dentro de una macro (`RESIZE_TYPE(Char, Normalized)`), asi que esos
 * identificadores NO existen en el texto y ninguna busqueda por patron los encuentra.
 * Aliasar solo «lo usado» dejaba 92 errores imposibles de explicar.
 *
 * EXCLUIDO 1: las cabeceras `MTL4*.hpp`. Son la API de nueva generacion de Metal y
 * viven en el namespace `MTL4`, no en `MTL`; colarlas daba 89 errores del tipo
 * «no type named 'Archive' in namespace 'MTL'».
 *
 * EXCLUIDO 2: los nombres `MTL*` que declara el propio Blender (MTLBuffer,
 * MTLTexture, MTLBufferRange, MTLContext...). No es cosmetico: `MTLBufferRange` es
 * una struct de mtl_memory.hh, y un alias al `MTL::BufferRange` de Apple la habria
 * tapado en silencio. Cuando el nombre choca se escribe explicito (`MTL::Buffer *`).
 */

/* --- Tipos y descriptores (327) --- */
using MTLAccelerationStructure = MTL::AccelerationStructure;
using MTLAccelerationStructureBoundingBoxGeometryDescriptor = MTL::AccelerationStructureBoundingBoxGeometryDescriptor;
using MTLAccelerationStructureCommandEncoder = MTL::AccelerationStructureCommandEncoder;
using MTLAccelerationStructureCurveGeometryDescriptor = MTL::AccelerationStructureCurveGeometryDescriptor;
using MTLAccelerationStructureDescriptor = MTL::AccelerationStructureDescriptor;
using MTLAccelerationStructureGeometryDescriptor = MTL::AccelerationStructureGeometryDescriptor;
using MTLAccelerationStructureInstanceDescriptor = MTL::AccelerationStructureInstanceDescriptor;
using MTLAccelerationStructureInstanceDescriptorType = MTL::AccelerationStructureInstanceDescriptorType;
using MTLAccelerationStructureInstanceOptions = MTL::AccelerationStructureInstanceOptions;
using MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor = MTL::AccelerationStructureMotionBoundingBoxGeometryDescriptor;
using MTLAccelerationStructureMotionCurveGeometryDescriptor = MTL::AccelerationStructureMotionCurveGeometryDescriptor;
using MTLAccelerationStructureMotionInstanceDescriptor = MTL::AccelerationStructureMotionInstanceDescriptor;
using MTLAccelerationStructureMotionTriangleGeometryDescriptor = MTL::AccelerationStructureMotionTriangleGeometryDescriptor;
using MTLAccelerationStructurePassDescriptor = MTL::AccelerationStructurePassDescriptor;
using MTLAccelerationStructurePassSampleBufferAttachmentDescriptor = MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor;
using MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray = MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray;
using MTLAccelerationStructureRefitOptions = MTL::AccelerationStructureRefitOptions;
using MTLAccelerationStructureSizes = MTL::AccelerationStructureSizes;
using MTLAccelerationStructureTriangleGeometryDescriptor = MTL::AccelerationStructureTriangleGeometryDescriptor;
using MTLAccelerationStructureUsage = MTL::AccelerationStructureUsage;
using MTLAccelerationStructureUserIDInstanceDescriptor = MTL::AccelerationStructureUserIDInstanceDescriptor;
using MTLAllocation = MTL::Allocation;
using MTLArchitecture = MTL::Architecture;
using MTLArgument = MTL::Argument;
using MTLArgumentBuffersTier = MTL::ArgumentBuffersTier;
using MTLArgumentDescriptor = MTL::ArgumentDescriptor;
using MTLArgumentEncoder = MTL::ArgumentEncoder;
using MTLArgumentType = MTL::ArgumentType;
using MTLArrayType = MTL::ArrayType;
using MTLAttribute = MTL::Attribute;
using MTLAttributeDescriptor = MTL::AttributeDescriptor;
using MTLAttributeDescriptorArray = MTL::AttributeDescriptorArray;
using MTLAttributeFormat = MTL::AttributeFormat;
using MTLAxisAlignedBoundingBox = MTL::AxisAlignedBoundingBox;
using MTLBarrierScope = MTL::BarrierScope;
using MTLBinaryArchive = MTL::BinaryArchive;
using MTLBinaryArchiveDescriptor = MTL::BinaryArchiveDescriptor;
using MTLBinaryArchiveError = MTL::BinaryArchiveError;
using MTLBinding = MTL::Binding;
using MTLBindingAccess = MTL::BindingAccess;
using MTLBindingType = MTL::BindingType;
using MTLBlendFactor = MTL::BlendFactor;
using MTLBlendOperation = MTL::BlendOperation;
using MTLBlitCommandEncoder = MTL::BlitCommandEncoder;
using MTLBlitOption = MTL::BlitOption;
using MTLBlitPassDescriptor = MTL::BlitPassDescriptor;
using MTLBlitPassSampleBufferAttachmentDescriptor = MTL::BlitPassSampleBufferAttachmentDescriptor;
using MTLBlitPassSampleBufferAttachmentDescriptorArray = MTL::BlitPassSampleBufferAttachmentDescriptorArray;
using MTLBufferBinding = MTL::BufferBinding;
using MTLBufferLayoutDescriptor = MTL::BufferLayoutDescriptor;
using MTLBufferLayoutDescriptorArray = MTL::BufferLayoutDescriptorArray;
using MTLBufferSparseTier = MTL::BufferSparseTier;
using MTLCPUCacheMode = MTL::CPUCacheMode;
using MTLCaptureDescriptor = MTL::CaptureDescriptor;
using MTLCaptureDestination = MTL::CaptureDestination;
using MTLCaptureError = MTL::CaptureError;
using MTLCaptureManager = MTL::CaptureManager;
using MTLCaptureScope = MTL::CaptureScope;
using MTLClearColor = MTL::ClearColor;
using MTLColorWriteMask = MTL::ColorWriteMask;
using MTLCommandBuffer = MTL::CommandBuffer;
using MTLCommandBufferDescriptor = MTL::CommandBufferDescriptor;
using MTLCommandBufferEncoderInfo = MTL::CommandBufferEncoderInfo;
using MTLCommandBufferError = MTL::CommandBufferError;
using MTLCommandBufferErrorOption = MTL::CommandBufferErrorOption;
using MTLCommandBufferStatus = MTL::CommandBufferStatus;
using MTLCommandEncoder = MTL::CommandEncoder;
using MTLCommandEncoderErrorState = MTL::CommandEncoderErrorState;
using MTLCommandQueue = MTL::CommandQueue;
using MTLCommandQueueDescriptor = MTL::CommandQueueDescriptor;
using MTLCompareFunction = MTL::CompareFunction;
using MTLCompileOptions = MTL::CompileOptions;
using MTLCompileSymbolVisibility = MTL::CompileSymbolVisibility;
using MTLComponentTransform = MTL::ComponentTransform;
using MTLComputeCommandEncoder = MTL::ComputeCommandEncoder;
using MTLComputePassDescriptor = MTL::ComputePassDescriptor;
using MTLComputePassSampleBufferAttachmentDescriptor = MTL::ComputePassSampleBufferAttachmentDescriptor;
using MTLComputePassSampleBufferAttachmentDescriptorArray = MTL::ComputePassSampleBufferAttachmentDescriptorArray;
using MTLComputePipelineDescriptor = MTL::ComputePipelineDescriptor;
using MTLComputePipelineReflection = MTL::ComputePipelineReflection;
using MTLComputePipelineState = MTL::ComputePipelineState;
using MTLCounter = MTL::Counter;
using MTLCounterResultStageUtilization = MTL::CounterResultStageUtilization;
using MTLCounterResultStatistic = MTL::CounterResultStatistic;
using MTLCounterResultTimestamp = MTL::CounterResultTimestamp;
using MTLCounterSampleBuffer = MTL::CounterSampleBuffer;
using MTLCounterSampleBufferDescriptor = MTL::CounterSampleBufferDescriptor;
using MTLCounterSampleBufferError = MTL::CounterSampleBufferError;
using MTLCounterSamplingPoint = MTL::CounterSamplingPoint;
using MTLCounterSet = MTL::CounterSet;
using MTLCullMode = MTL::CullMode;
using MTLCurveBasis = MTL::CurveBasis;
using MTLCurveEndCaps = MTL::CurveEndCaps;
using MTLCurveType = MTL::CurveType;
using MTLDataType = MTL::DataType;
using MTLDepthClipMode = MTL::DepthClipMode;
using MTLDepthStencilDescriptor = MTL::DepthStencilDescriptor;
using MTLDepthStencilState = MTL::DepthStencilState;
using MTLDevice = MTL::Device;
using MTLDeviceError = MTL::DeviceError;
using MTLDeviceLocation = MTL::DeviceLocation;
using MTLDispatchThreadgroupsIndirectArguments = MTL::DispatchThreadgroupsIndirectArguments;
using MTLDispatchThreadsIndirectArguments = MTL::DispatchThreadsIndirectArguments;
using MTLDispatchType = MTL::DispatchType;
using MTLDrawIndexedPrimitivesIndirectArguments = MTL::DrawIndexedPrimitivesIndirectArguments;
using MTLDrawPatchIndirectArguments = MTL::DrawPatchIndirectArguments;
using MTLDrawPrimitivesIndirectArguments = MTL::DrawPrimitivesIndirectArguments;
using MTLDrawable = MTL::Drawable;
using MTLDynamicLibrary = MTL::DynamicLibrary;
using MTLDynamicLibraryError = MTL::DynamicLibraryError;
using MTLEvent = MTL::Event;
using MTLFeatureSet = MTL::FeatureSet;
using MTLFunction = MTL::Function;
using MTLFunctionConstant = MTL::FunctionConstant;
using MTLFunctionConstantValues = MTL::FunctionConstantValues;
using MTLFunctionDescriptor = MTL::FunctionDescriptor;
using MTLFunctionHandle = MTL::FunctionHandle;
using MTLFunctionLog = MTL::FunctionLog;
using MTLFunctionLogDebugLocation = MTL::FunctionLogDebugLocation;
using MTLFunctionLogType = MTL::FunctionLogType;
using MTLFunctionOptions = MTL::FunctionOptions;
using MTLFunctionReflection = MTL::FunctionReflection;
using MTLFunctionStitchingAttribute = MTL::FunctionStitchingAttribute;
using MTLFunctionStitchingAttributeAlwaysInline = MTL::FunctionStitchingAttributeAlwaysInline;
using MTLFunctionStitchingFunctionNode = MTL::FunctionStitchingFunctionNode;
using MTLFunctionStitchingGraph = MTL::FunctionStitchingGraph;
using MTLFunctionStitchingInputNode = MTL::FunctionStitchingInputNode;
using MTLFunctionStitchingNode = MTL::FunctionStitchingNode;
using MTLFunctionType = MTL::FunctionType;
using MTLGPUFamily = MTL::GPUFamily;
using MTLHazardTrackingMode = MTL::HazardTrackingMode;
using MTLHeap = MTL::Heap;
using MTLHeapDescriptor = MTL::HeapDescriptor;
using MTLHeapType = MTL::HeapType;
using MTLIOCommandBuffer = MTL::IOCommandBuffer;
using MTLIOCommandQueue = MTL::IOCommandQueue;
using MTLIOCommandQueueDescriptor = MTL::IOCommandQueueDescriptor;
using MTLIOCommandQueueType = MTL::IOCommandQueueType;
using MTLIOCompressionMethod = MTL::IOCompressionMethod;
using MTLIOCompressionStatus = MTL::IOCompressionStatus;
using MTLIOError = MTL::IOError;
using MTLIOFileHandle = MTL::IOFileHandle;
using MTLIOPriority = MTL::IOPriority;
using MTLIOScratchBuffer = MTL::IOScratchBuffer;
using MTLIOScratchBufferAllocator = MTL::IOScratchBufferAllocator;
using MTLIOStatus = MTL::IOStatus;
using MTLIndexType = MTL::IndexType;
using MTLIndirectAccelerationStructureInstanceDescriptor = MTL::IndirectAccelerationStructureInstanceDescriptor;
using MTLIndirectAccelerationStructureMotionInstanceDescriptor = MTL::IndirectAccelerationStructureMotionInstanceDescriptor;
using MTLIndirectCommandBuffer = MTL::IndirectCommandBuffer;
using MTLIndirectCommandBufferDescriptor = MTL::IndirectCommandBufferDescriptor;
using MTLIndirectCommandBufferExecutionRange = MTL::IndirectCommandBufferExecutionRange;
using MTLIndirectCommandType = MTL::IndirectCommandType;
using MTLIndirectComputeCommand = MTL::IndirectComputeCommand;
using MTLIndirectInstanceAccelerationStructureDescriptor = MTL::IndirectInstanceAccelerationStructureDescriptor;
using MTLIndirectRenderCommand = MTL::IndirectRenderCommand;
using MTLInstanceAccelerationStructureDescriptor = MTL::InstanceAccelerationStructureDescriptor;
using MTLIntersectionFunctionBufferArguments = MTL::IntersectionFunctionBufferArguments;
using MTLIntersectionFunctionDescriptor = MTL::IntersectionFunctionDescriptor;
using MTLIntersectionFunctionSignature = MTL::IntersectionFunctionSignature;
using MTLIntersectionFunctionTable = MTL::IntersectionFunctionTable;
using MTLIntersectionFunctionTableDescriptor = MTL::IntersectionFunctionTableDescriptor;
using MTLLanguageVersion = MTL::LanguageVersion;
using MTLLibrary = MTL::Library;
using MTLLibraryError = MTL::LibraryError;
using MTLLibraryOptimizationLevel = MTL::LibraryOptimizationLevel;
using MTLLibraryType = MTL::LibraryType;
using MTLLinkedFunctions = MTL::LinkedFunctions;
using MTLLoadAction = MTL::LoadAction;
using MTLLogContainer = MTL::LogContainer;
using MTLLogLevel = MTL::LogLevel;
using MTLLogState = MTL::LogState;
using MTLLogStateDescriptor = MTL::LogStateDescriptor;
using MTLLogStateError = MTL::LogStateError;
using MTLLogicalToPhysicalColorAttachmentMap = MTL::LogicalToPhysicalColorAttachmentMap;
using MTLMapIndirectArguments = MTL::MapIndirectArguments;
using MTLMathFloatingPointFunctions = MTL::MathFloatingPointFunctions;
using MTLMathMode = MTL::MathMode;
using MTLMatrixLayout = MTL::MatrixLayout;
using MTLMeshRenderPipelineDescriptor = MTL::MeshRenderPipelineDescriptor;
using MTLMotionBorderMode = MTL::MotionBorderMode;
using MTLMotionKeyframeData = MTL::MotionKeyframeData;
using MTLMultisampleDepthResolveFilter = MTL::MultisampleDepthResolveFilter;
using MTLMultisampleStencilResolveFilter = MTL::MultisampleStencilResolveFilter;
using MTLMutability = MTL::Mutability;
using MTLObjectPayloadBinding = MTL::ObjectPayloadBinding;
using MTLOrigin = MTL::Origin;
using MTLPackedFloat3 = MTL::PackedFloat3;
using MTLPackedFloat4x3 = MTL::PackedFloat4x3;
using MTLPackedFloatQuaternion = MTL::PackedFloatQuaternion;
using MTLParallelRenderCommandEncoder = MTL::ParallelRenderCommandEncoder;
using MTLPatchType = MTL::PatchType;
using MTLPipelineBufferDescriptor = MTL::PipelineBufferDescriptor;
using MTLPipelineBufferDescriptorArray = MTL::PipelineBufferDescriptorArray;
using MTLPipelineOption = MTL::PipelineOption;
using MTLPixelFormat = MTL::PixelFormat;
using MTLPointerType = MTL::PointerType;
using MTLPrimitiveAccelerationStructureDescriptor = MTL::PrimitiveAccelerationStructureDescriptor;
using MTLPrimitiveTopologyClass = MTL::PrimitiveTopologyClass;
using MTLPrimitiveType = MTL::PrimitiveType;
using MTLPurgeableState = MTL::PurgeableState;
using MTLQuadTessellationFactorsHalf = MTL::QuadTessellationFactorsHalf;
using MTLRasterizationRateLayerArray = MTL::RasterizationRateLayerArray;
using MTLRasterizationRateLayerDescriptor = MTL::RasterizationRateLayerDescriptor;
using MTLRasterizationRateMap = MTL::RasterizationRateMap;
using MTLRasterizationRateMapDescriptor = MTL::RasterizationRateMapDescriptor;
using MTLRasterizationRateSampleArray = MTL::RasterizationRateSampleArray;
using MTLReadWriteTextureTier = MTL::ReadWriteTextureTier;
using MTLRegion = MTL::Region;
using MTLRenderCommandEncoder = MTL::RenderCommandEncoder;
using MTLRenderPassAttachmentDescriptor = MTL::RenderPassAttachmentDescriptor;
using MTLRenderPassColorAttachmentDescriptor = MTL::RenderPassColorAttachmentDescriptor;
using MTLRenderPassColorAttachmentDescriptorArray = MTL::RenderPassColorAttachmentDescriptorArray;
using MTLRenderPassDepthAttachmentDescriptor = MTL::RenderPassDepthAttachmentDescriptor;
using MTLRenderPassDescriptor = MTL::RenderPassDescriptor;
using MTLRenderPassSampleBufferAttachmentDescriptor = MTL::RenderPassSampleBufferAttachmentDescriptor;
using MTLRenderPassSampleBufferAttachmentDescriptorArray = MTL::RenderPassSampleBufferAttachmentDescriptorArray;
using MTLRenderPassStencilAttachmentDescriptor = MTL::RenderPassStencilAttachmentDescriptor;
using MTLRenderPipelineColorAttachmentDescriptor = MTL::RenderPipelineColorAttachmentDescriptor;
using MTLRenderPipelineColorAttachmentDescriptorArray = MTL::RenderPipelineColorAttachmentDescriptorArray;
using MTLRenderPipelineDescriptor = MTL::RenderPipelineDescriptor;
using MTLRenderPipelineFunctionsDescriptor = MTL::RenderPipelineFunctionsDescriptor;
using MTLRenderPipelineReflection = MTL::RenderPipelineReflection;
using MTLRenderPipelineState = MTL::RenderPipelineState;
using MTLRenderStages = MTL::RenderStages;
using MTLResidencySet = MTL::ResidencySet;
using MTLResidencySetDescriptor = MTL::ResidencySetDescriptor;
using MTLResource = MTL::Resource;
using MTLResourceID = MTL::ResourceID;
using MTLResourceOptions = MTL::ResourceOptions;
using MTLResourceStateCommandEncoder = MTL::ResourceStateCommandEncoder;
using MTLResourceStatePassDescriptor = MTL::ResourceStatePassDescriptor;
using MTLResourceStatePassSampleBufferAttachmentDescriptor = MTL::ResourceStatePassSampleBufferAttachmentDescriptor;
using MTLResourceStatePassSampleBufferAttachmentDescriptorArray = MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray;
using MTLResourceUsage = MTL::ResourceUsage;
using MTLResourceViewPool = MTL::ResourceViewPool;
using MTLResourceViewPoolDescriptor = MTL::ResourceViewPoolDescriptor;
using MTLSamplePosition = MTL::SamplePosition;
using MTLSamplerAddressMode = MTL::SamplerAddressMode;
using MTLSamplerBorderColor = MTL::SamplerBorderColor;
using MTLSamplerDescriptor = MTL::SamplerDescriptor;
using MTLSamplerMinMagFilter = MTL::SamplerMinMagFilter;
using MTLSamplerMipFilter = MTL::SamplerMipFilter;
using MTLSamplerReductionMode = MTL::SamplerReductionMode;
using MTLScissorRect = MTL::ScissorRect;
using MTLShaderValidation = MTL::ShaderValidation;
using MTLSharedEvent = MTL::SharedEvent;
using MTLSharedEventHandle = MTL::SharedEventHandle;
using MTLSharedEventListener = MTL::SharedEventListener;
using MTLSharedTextureHandle = MTL::SharedTextureHandle;
using MTLSize = MTL::Size;
using MTLSizeAndAlign = MTL::SizeAndAlign;
using MTLSparsePageSize = MTL::SparsePageSize;
using MTLSparseTextureMappingMode = MTL::SparseTextureMappingMode;
using MTLSparseTextureRegionAlignmentMode = MTL::SparseTextureRegionAlignmentMode;
using MTLStageInRegionIndirectArguments = MTL::StageInRegionIndirectArguments;
using MTLStageInputOutputDescriptor = MTL::StageInputOutputDescriptor;
using MTLStages = MTL::Stages;
using MTLStencilDescriptor = MTL::StencilDescriptor;
using MTLStencilOperation = MTL::StencilOperation;
using MTLStepFunction = MTL::StepFunction;
using MTLStitchedLibraryDescriptor = MTL::StitchedLibraryDescriptor;
using MTLStitchedLibraryOptions = MTL::StitchedLibraryOptions;
using MTLStorageMode = MTL::StorageMode;
using MTLStoreAction = MTL::StoreAction;
using MTLStoreActionOptions = MTL::StoreActionOptions;
using MTLStructMember = MTL::StructMember;
using MTLStructType = MTL::StructType;
using MTLTensor = MTL::Tensor;
using MTLTensorBinding = MTL::TensorBinding;
using MTLTensorDataType = MTL::TensorDataType;
using MTLTensorDescriptor = MTL::TensorDescriptor;
using MTLTensorError = MTL::TensorError;
using MTLTensorExtents = MTL::TensorExtents;
using MTLTensorReferenceType = MTL::TensorReferenceType;
using MTLTensorUsage = MTL::TensorUsage;
using MTLTessellationControlPointIndexType = MTL::TessellationControlPointIndexType;
using MTLTessellationFactorFormat = MTL::TessellationFactorFormat;
using MTLTessellationFactorStepFunction = MTL::TessellationFactorStepFunction;
using MTLTessellationPartitionMode = MTL::TessellationPartitionMode;
using MTLTextureCompressionType = MTL::TextureCompressionType;
using MTLTextureDescriptor = MTL::TextureDescriptor;
using MTLTextureReferenceType = MTL::TextureReferenceType;
using MTLTextureSparseTier = MTL::TextureSparseTier;
using MTLTextureSwizzle = MTL::TextureSwizzle;
using MTLTextureSwizzleChannels = MTL::TextureSwizzleChannels;
using MTLTextureType = MTL::TextureType;
using MTLTextureUsage = MTL::TextureUsage;
using MTLTextureViewDescriptor = MTL::TextureViewDescriptor;
using MTLTextureViewPool = MTL::TextureViewPool;
using MTLThreadgroupBinding = MTL::ThreadgroupBinding;
using MTLTileRenderPipelineColorAttachmentDescriptor = MTL::TileRenderPipelineColorAttachmentDescriptor;
using MTLTileRenderPipelineColorAttachmentDescriptorArray = MTL::TileRenderPipelineColorAttachmentDescriptorArray;
using MTLTileRenderPipelineDescriptor = MTL::TileRenderPipelineDescriptor;
using MTLTransformType = MTL::TransformType;
using MTLTriangleFillMode = MTL::TriangleFillMode;
using MTLTriangleTessellationFactorsHalf = MTL::TriangleTessellationFactorsHalf;
using MTLType = MTL::Type;
using MTLVertexAmplificationViewMapping = MTL::VertexAmplificationViewMapping;
using MTLVertexAttribute = MTL::VertexAttribute;
using MTLVertexAttributeDescriptor = MTL::VertexAttributeDescriptor;
using MTLVertexAttributeDescriptorArray = MTL::VertexAttributeDescriptorArray;
using MTLVertexBufferLayoutDescriptor = MTL::VertexBufferLayoutDescriptor;
using MTLVertexBufferLayoutDescriptorArray = MTL::VertexBufferLayoutDescriptorArray;
using MTLVertexFormat = MTL::VertexFormat;
using MTLVertexStepFunction = MTL::VertexStepFunction;
using MTLViewport = MTL::Viewport;
using MTLVisibilityResultMode = MTL::VisibilityResultMode;
using MTLVisibilityResultType = MTL::VisibilityResultType;
using MTLVisibleFunctionTable = MTL::VisibleFunctionTable;
using MTLVisibleFunctionTableDescriptor = MTL::VisibleFunctionTableDescriptor;
using MTLWinding = MTL::Winding;

/* --- Constantes de enumerado (868) --- */
constexpr auto MTLAccelerationStructureInstanceDescriptorTypeDefault = MTL::AccelerationStructureInstanceDescriptorTypeDefault;
constexpr auto MTLAccelerationStructureInstanceDescriptorTypeIndirect = MTL::AccelerationStructureInstanceDescriptorTypeIndirect;
constexpr auto MTLAccelerationStructureInstanceDescriptorTypeIndirectMotion = MTL::AccelerationStructureInstanceDescriptorTypeIndirectMotion;
constexpr auto MTLAccelerationStructureInstanceDescriptorTypeMotion = MTL::AccelerationStructureInstanceDescriptorTypeMotion;
constexpr auto MTLAccelerationStructureInstanceDescriptorTypeUserID = MTL::AccelerationStructureInstanceDescriptorTypeUserID;
constexpr auto MTLAccelerationStructureInstanceOptionDisableTriangleCulling = MTL::AccelerationStructureInstanceOptionDisableTriangleCulling;
constexpr auto MTLAccelerationStructureInstanceOptionNonOpaque = MTL::AccelerationStructureInstanceOptionNonOpaque;
constexpr auto MTLAccelerationStructureInstanceOptionNone = MTL::AccelerationStructureInstanceOptionNone;
constexpr auto MTLAccelerationStructureInstanceOptionOpaque = MTL::AccelerationStructureInstanceOptionOpaque;
constexpr auto MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise = MTL::AccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise;
constexpr auto MTLAccelerationStructureRefitOptionPerPrimitiveData = MTL::AccelerationStructureRefitOptionPerPrimitiveData;
constexpr auto MTLAccelerationStructureRefitOptionVertexData = MTL::AccelerationStructureRefitOptionVertexData;
constexpr auto MTLAccelerationStructureUsageExtendedLimits = MTL::AccelerationStructureUsageExtendedLimits;
constexpr auto MTLAccelerationStructureUsageMinimizeMemory = MTL::AccelerationStructureUsageMinimizeMemory;
constexpr auto MTLAccelerationStructureUsageNone = MTL::AccelerationStructureUsageNone;
constexpr auto MTLAccelerationStructureUsagePreferFastBuild = MTL::AccelerationStructureUsagePreferFastBuild;
constexpr auto MTLAccelerationStructureUsagePreferFastIntersection = MTL::AccelerationStructureUsagePreferFastIntersection;
constexpr auto MTLAccelerationStructureUsageRefit = MTL::AccelerationStructureUsageRefit;
constexpr auto MTLArgumentAccessReadOnly = MTL::ArgumentAccessReadOnly;
constexpr auto MTLArgumentAccessReadWrite = MTL::ArgumentAccessReadWrite;
constexpr auto MTLArgumentAccessWriteOnly = MTL::ArgumentAccessWriteOnly;
constexpr auto MTLArgumentBuffersTier1 = MTL::ArgumentBuffersTier1;
constexpr auto MTLArgumentBuffersTier2 = MTL::ArgumentBuffersTier2;
constexpr auto MTLArgumentTypeBuffer = MTL::ArgumentTypeBuffer;
constexpr auto MTLArgumentTypeImageblock = MTL::ArgumentTypeImageblock;
constexpr auto MTLArgumentTypeImageblockData = MTL::ArgumentTypeImageblockData;
constexpr auto MTLArgumentTypeInstanceAccelerationStructure = MTL::ArgumentTypeInstanceAccelerationStructure;
constexpr auto MTLArgumentTypeIntersectionFunctionTable = MTL::ArgumentTypeIntersectionFunctionTable;
constexpr auto MTLArgumentTypePrimitiveAccelerationStructure = MTL::ArgumentTypePrimitiveAccelerationStructure;
constexpr auto MTLArgumentTypeSampler = MTL::ArgumentTypeSampler;
constexpr auto MTLArgumentTypeTexture = MTL::ArgumentTypeTexture;
constexpr auto MTLArgumentTypeThreadgroupMemory = MTL::ArgumentTypeThreadgroupMemory;
constexpr auto MTLArgumentTypeVisibleFunctionTable = MTL::ArgumentTypeVisibleFunctionTable;
constexpr auto MTLAttributeFormatChar = MTL::AttributeFormatChar;
constexpr auto MTLAttributeFormatChar2 = MTL::AttributeFormatChar2;
constexpr auto MTLAttributeFormatChar2Normalized = MTL::AttributeFormatChar2Normalized;
constexpr auto MTLAttributeFormatChar3 = MTL::AttributeFormatChar3;
constexpr auto MTLAttributeFormatChar3Normalized = MTL::AttributeFormatChar3Normalized;
constexpr auto MTLAttributeFormatChar4 = MTL::AttributeFormatChar4;
constexpr auto MTLAttributeFormatChar4Normalized = MTL::AttributeFormatChar4Normalized;
constexpr auto MTLAttributeFormatCharNormalized = MTL::AttributeFormatCharNormalized;
constexpr auto MTLAttributeFormatFloat = MTL::AttributeFormatFloat;
constexpr auto MTLAttributeFormatFloat2 = MTL::AttributeFormatFloat2;
constexpr auto MTLAttributeFormatFloat3 = MTL::AttributeFormatFloat3;
constexpr auto MTLAttributeFormatFloat4 = MTL::AttributeFormatFloat4;
constexpr auto MTLAttributeFormatFloatRG11B10 = MTL::AttributeFormatFloatRG11B10;
constexpr auto MTLAttributeFormatFloatRGB9E5 = MTL::AttributeFormatFloatRGB9E5;
constexpr auto MTLAttributeFormatHalf = MTL::AttributeFormatHalf;
constexpr auto MTLAttributeFormatHalf2 = MTL::AttributeFormatHalf2;
constexpr auto MTLAttributeFormatHalf3 = MTL::AttributeFormatHalf3;
constexpr auto MTLAttributeFormatHalf4 = MTL::AttributeFormatHalf4;
constexpr auto MTLAttributeFormatInt = MTL::AttributeFormatInt;
constexpr auto MTLAttributeFormatInt1010102Normalized = MTL::AttributeFormatInt1010102Normalized;
constexpr auto MTLAttributeFormatInt2 = MTL::AttributeFormatInt2;
constexpr auto MTLAttributeFormatInt3 = MTL::AttributeFormatInt3;
constexpr auto MTLAttributeFormatInt4 = MTL::AttributeFormatInt4;
constexpr auto MTLAttributeFormatInvalid = MTL::AttributeFormatInvalid;
constexpr auto MTLAttributeFormatShort = MTL::AttributeFormatShort;
constexpr auto MTLAttributeFormatShort2 = MTL::AttributeFormatShort2;
constexpr auto MTLAttributeFormatShort2Normalized = MTL::AttributeFormatShort2Normalized;
constexpr auto MTLAttributeFormatShort3 = MTL::AttributeFormatShort3;
constexpr auto MTLAttributeFormatShort3Normalized = MTL::AttributeFormatShort3Normalized;
constexpr auto MTLAttributeFormatShort4 = MTL::AttributeFormatShort4;
constexpr auto MTLAttributeFormatShort4Normalized = MTL::AttributeFormatShort4Normalized;
constexpr auto MTLAttributeFormatShortNormalized = MTL::AttributeFormatShortNormalized;
constexpr auto MTLAttributeFormatUChar = MTL::AttributeFormatUChar;
constexpr auto MTLAttributeFormatUChar2 = MTL::AttributeFormatUChar2;
constexpr auto MTLAttributeFormatUChar2Normalized = MTL::AttributeFormatUChar2Normalized;
constexpr auto MTLAttributeFormatUChar3 = MTL::AttributeFormatUChar3;
constexpr auto MTLAttributeFormatUChar3Normalized = MTL::AttributeFormatUChar3Normalized;
constexpr auto MTLAttributeFormatUChar4 = MTL::AttributeFormatUChar4;
constexpr auto MTLAttributeFormatUChar4Normalized = MTL::AttributeFormatUChar4Normalized;
constexpr auto MTLAttributeFormatUChar4Normalized_BGRA = MTL::AttributeFormatUChar4Normalized_BGRA;
constexpr auto MTLAttributeFormatUCharNormalized = MTL::AttributeFormatUCharNormalized;
constexpr auto MTLAttributeFormatUInt = MTL::AttributeFormatUInt;
constexpr auto MTLAttributeFormatUInt1010102Normalized = MTL::AttributeFormatUInt1010102Normalized;
constexpr auto MTLAttributeFormatUInt2 = MTL::AttributeFormatUInt2;
constexpr auto MTLAttributeFormatUInt3 = MTL::AttributeFormatUInt3;
constexpr auto MTLAttributeFormatUInt4 = MTL::AttributeFormatUInt4;
constexpr auto MTLAttributeFormatUShort = MTL::AttributeFormatUShort;
constexpr auto MTLAttributeFormatUShort2 = MTL::AttributeFormatUShort2;
constexpr auto MTLAttributeFormatUShort2Normalized = MTL::AttributeFormatUShort2Normalized;
constexpr auto MTLAttributeFormatUShort3 = MTL::AttributeFormatUShort3;
constexpr auto MTLAttributeFormatUShort3Normalized = MTL::AttributeFormatUShort3Normalized;
constexpr auto MTLAttributeFormatUShort4 = MTL::AttributeFormatUShort4;
constexpr auto MTLAttributeFormatUShort4Normalized = MTL::AttributeFormatUShort4Normalized;
constexpr auto MTLAttributeFormatUShortNormalized = MTL::AttributeFormatUShortNormalized;
constexpr auto MTLBarrierScopeBuffers = MTL::BarrierScopeBuffers;
constexpr auto MTLBarrierScopeRenderTargets = MTL::BarrierScopeRenderTargets;
constexpr auto MTLBarrierScopeTextures = MTL::BarrierScopeTextures;
constexpr auto MTLBinaryArchiveErrorCompilationFailure = MTL::BinaryArchiveErrorCompilationFailure;
constexpr auto MTLBinaryArchiveErrorInternalError = MTL::BinaryArchiveErrorInternalError;
constexpr auto MTLBinaryArchiveErrorInvalidFile = MTL::BinaryArchiveErrorInvalidFile;
constexpr auto MTLBinaryArchiveErrorNone = MTL::BinaryArchiveErrorNone;
constexpr auto MTLBinaryArchiveErrorUnexpectedElement = MTL::BinaryArchiveErrorUnexpectedElement;
constexpr auto MTLBindingAccessReadOnly = MTL::BindingAccessReadOnly;
constexpr auto MTLBindingAccessReadWrite = MTL::BindingAccessReadWrite;
constexpr auto MTLBindingAccessWriteOnly = MTL::BindingAccessWriteOnly;
constexpr auto MTLBindingTypeBuffer = MTL::BindingTypeBuffer;
constexpr auto MTLBindingTypeImageblock = MTL::BindingTypeImageblock;
constexpr auto MTLBindingTypeImageblockData = MTL::BindingTypeImageblockData;
constexpr auto MTLBindingTypeInstanceAccelerationStructure = MTL::BindingTypeInstanceAccelerationStructure;
constexpr auto MTLBindingTypeIntersectionFunctionTable = MTL::BindingTypeIntersectionFunctionTable;
constexpr auto MTLBindingTypeObjectPayload = MTL::BindingTypeObjectPayload;
constexpr auto MTLBindingTypePrimitiveAccelerationStructure = MTL::BindingTypePrimitiveAccelerationStructure;
constexpr auto MTLBindingTypeSampler = MTL::BindingTypeSampler;
constexpr auto MTLBindingTypeTensor = MTL::BindingTypeTensor;
constexpr auto MTLBindingTypeTexture = MTL::BindingTypeTexture;
constexpr auto MTLBindingTypeThreadgroupMemory = MTL::BindingTypeThreadgroupMemory;
constexpr auto MTLBindingTypeVisibleFunctionTable = MTL::BindingTypeVisibleFunctionTable;
constexpr auto MTLBlendFactorBlendAlpha = MTL::BlendFactorBlendAlpha;
constexpr auto MTLBlendFactorBlendColor = MTL::BlendFactorBlendColor;
constexpr auto MTLBlendFactorDestinationAlpha = MTL::BlendFactorDestinationAlpha;
constexpr auto MTLBlendFactorDestinationColor = MTL::BlendFactorDestinationColor;
constexpr auto MTLBlendFactorOne = MTL::BlendFactorOne;
constexpr auto MTLBlendFactorOneMinusBlendAlpha = MTL::BlendFactorOneMinusBlendAlpha;
constexpr auto MTLBlendFactorOneMinusBlendColor = MTL::BlendFactorOneMinusBlendColor;
constexpr auto MTLBlendFactorOneMinusDestinationAlpha = MTL::BlendFactorOneMinusDestinationAlpha;
constexpr auto MTLBlendFactorOneMinusDestinationColor = MTL::BlendFactorOneMinusDestinationColor;
constexpr auto MTLBlendFactorOneMinusSource1Alpha = MTL::BlendFactorOneMinusSource1Alpha;
constexpr auto MTLBlendFactorOneMinusSource1Color = MTL::BlendFactorOneMinusSource1Color;
constexpr auto MTLBlendFactorOneMinusSourceAlpha = MTL::BlendFactorOneMinusSourceAlpha;
constexpr auto MTLBlendFactorOneMinusSourceColor = MTL::BlendFactorOneMinusSourceColor;
constexpr auto MTLBlendFactorSource1Alpha = MTL::BlendFactorSource1Alpha;
constexpr auto MTLBlendFactorSource1Color = MTL::BlendFactorSource1Color;
constexpr auto MTLBlendFactorSourceAlpha = MTL::BlendFactorSourceAlpha;
constexpr auto MTLBlendFactorSourceAlphaSaturated = MTL::BlendFactorSourceAlphaSaturated;
constexpr auto MTLBlendFactorSourceColor = MTL::BlendFactorSourceColor;
constexpr auto MTLBlendFactorUnspecialized = MTL::BlendFactorUnspecialized;
constexpr auto MTLBlendFactorZero = MTL::BlendFactorZero;
constexpr auto MTLBlendOperationAdd = MTL::BlendOperationAdd;
constexpr auto MTLBlendOperationMax = MTL::BlendOperationMax;
constexpr auto MTLBlendOperationMin = MTL::BlendOperationMin;
constexpr auto MTLBlendOperationReverseSubtract = MTL::BlendOperationReverseSubtract;
constexpr auto MTLBlendOperationSubtract = MTL::BlendOperationSubtract;
constexpr auto MTLBlendOperationUnspecialized = MTL::BlendOperationUnspecialized;
constexpr auto MTLBlitOptionDepthFromDepthStencil = MTL::BlitOptionDepthFromDepthStencil;
constexpr auto MTLBlitOptionNone = MTL::BlitOptionNone;
constexpr auto MTLBlitOptionRowLinearPVRTC = MTL::BlitOptionRowLinearPVRTC;
constexpr auto MTLBlitOptionStencilFromDepthStencil = MTL::BlitOptionStencilFromDepthStencil;
constexpr auto MTLBufferSparseTier1 = MTL::BufferSparseTier1;
constexpr auto MTLBufferSparseTierNone = MTL::BufferSparseTierNone;
constexpr auto MTLCPUCacheModeDefaultCache = MTL::CPUCacheModeDefaultCache;
constexpr auto MTLCPUCacheModeWriteCombined = MTL::CPUCacheModeWriteCombined;
constexpr auto MTLCaptureDestinationDeveloperTools = MTL::CaptureDestinationDeveloperTools;
constexpr auto MTLCaptureDestinationGPUTraceDocument = MTL::CaptureDestinationGPUTraceDocument;
constexpr auto MTLCaptureErrorAlreadyCapturing = MTL::CaptureErrorAlreadyCapturing;
constexpr auto MTLCaptureErrorInvalidDescriptor = MTL::CaptureErrorInvalidDescriptor;
constexpr auto MTLCaptureErrorNotSupported = MTL::CaptureErrorNotSupported;
constexpr auto MTLColorWriteMaskAll = MTL::ColorWriteMaskAll;
constexpr auto MTLColorWriteMaskAlpha = MTL::ColorWriteMaskAlpha;
constexpr auto MTLColorWriteMaskBlue = MTL::ColorWriteMaskBlue;
constexpr auto MTLColorWriteMaskGreen = MTL::ColorWriteMaskGreen;
constexpr auto MTLColorWriteMaskNone = MTL::ColorWriteMaskNone;
constexpr auto MTLColorWriteMaskRed = MTL::ColorWriteMaskRed;
constexpr auto MTLColorWriteMaskUnspecialized = MTL::ColorWriteMaskUnspecialized;
constexpr auto MTLCommandBufferErrorAccessRevoked = MTL::CommandBufferErrorAccessRevoked;
constexpr auto MTLCommandBufferErrorBlacklisted = MTL::CommandBufferErrorBlacklisted;
constexpr auto MTLCommandBufferErrorDeviceRemoved = MTL::CommandBufferErrorDeviceRemoved;
constexpr auto MTLCommandBufferErrorInternal = MTL::CommandBufferErrorInternal;
constexpr auto MTLCommandBufferErrorInvalidResource = MTL::CommandBufferErrorInvalidResource;
constexpr auto MTLCommandBufferErrorMemoryless = MTL::CommandBufferErrorMemoryless;
constexpr auto MTLCommandBufferErrorNone = MTL::CommandBufferErrorNone;
constexpr auto MTLCommandBufferErrorNotPermitted = MTL::CommandBufferErrorNotPermitted;
constexpr auto MTLCommandBufferErrorOptionEncoderExecutionStatus = MTL::CommandBufferErrorOptionEncoderExecutionStatus;
constexpr auto MTLCommandBufferErrorOptionNone = MTL::CommandBufferErrorOptionNone;
constexpr auto MTLCommandBufferErrorOutOfMemory = MTL::CommandBufferErrorOutOfMemory;
constexpr auto MTLCommandBufferErrorPageFault = MTL::CommandBufferErrorPageFault;
constexpr auto MTLCommandBufferErrorStackOverflow = MTL::CommandBufferErrorStackOverflow;
constexpr auto MTLCommandBufferErrorTimeout = MTL::CommandBufferErrorTimeout;
constexpr auto MTLCommandBufferStatusCommitted = MTL::CommandBufferStatusCommitted;
constexpr auto MTLCommandBufferStatusCompleted = MTL::CommandBufferStatusCompleted;
constexpr auto MTLCommandBufferStatusEnqueued = MTL::CommandBufferStatusEnqueued;
constexpr auto MTLCommandBufferStatusError = MTL::CommandBufferStatusError;
constexpr auto MTLCommandBufferStatusNotEnqueued = MTL::CommandBufferStatusNotEnqueued;
constexpr auto MTLCommandBufferStatusScheduled = MTL::CommandBufferStatusScheduled;
constexpr auto MTLCommandEncoderErrorStateAffected = MTL::CommandEncoderErrorStateAffected;
constexpr auto MTLCommandEncoderErrorStateCompleted = MTL::CommandEncoderErrorStateCompleted;
constexpr auto MTLCommandEncoderErrorStateFaulted = MTL::CommandEncoderErrorStateFaulted;
constexpr auto MTLCommandEncoderErrorStatePending = MTL::CommandEncoderErrorStatePending;
constexpr auto MTLCommandEncoderErrorStateUnknown = MTL::CommandEncoderErrorStateUnknown;
constexpr auto MTLCompareFunctionAlways = MTL::CompareFunctionAlways;
constexpr auto MTLCompareFunctionEqual = MTL::CompareFunctionEqual;
constexpr auto MTLCompareFunctionGreater = MTL::CompareFunctionGreater;
constexpr auto MTLCompareFunctionGreaterEqual = MTL::CompareFunctionGreaterEqual;
constexpr auto MTLCompareFunctionLess = MTL::CompareFunctionLess;
constexpr auto MTLCompareFunctionLessEqual = MTL::CompareFunctionLessEqual;
constexpr auto MTLCompareFunctionNever = MTL::CompareFunctionNever;
constexpr auto MTLCompareFunctionNotEqual = MTL::CompareFunctionNotEqual;
constexpr auto MTLCompileSymbolVisibilityDefault = MTL::CompileSymbolVisibilityDefault;
constexpr auto MTLCompileSymbolVisibilityHidden = MTL::CompileSymbolVisibilityHidden;
constexpr auto MTLCounterSampleBufferErrorInternal = MTL::CounterSampleBufferErrorInternal;
constexpr auto MTLCounterSampleBufferErrorInvalid = MTL::CounterSampleBufferErrorInvalid;
constexpr auto MTLCounterSampleBufferErrorOutOfMemory = MTL::CounterSampleBufferErrorOutOfMemory;
constexpr auto MTLCounterSamplingPointAtBlitBoundary = MTL::CounterSamplingPointAtBlitBoundary;
constexpr auto MTLCounterSamplingPointAtDispatchBoundary = MTL::CounterSamplingPointAtDispatchBoundary;
constexpr auto MTLCounterSamplingPointAtDrawBoundary = MTL::CounterSamplingPointAtDrawBoundary;
constexpr auto MTLCounterSamplingPointAtStageBoundary = MTL::CounterSamplingPointAtStageBoundary;
constexpr auto MTLCounterSamplingPointAtTileDispatchBoundary = MTL::CounterSamplingPointAtTileDispatchBoundary;
constexpr auto MTLCullModeBack = MTL::CullModeBack;
constexpr auto MTLCullModeFront = MTL::CullModeFront;
constexpr auto MTLCullModeNone = MTL::CullModeNone;
constexpr auto MTLCurveBasisBSpline = MTL::CurveBasisBSpline;
constexpr auto MTLCurveBasisBezier = MTL::CurveBasisBezier;
constexpr auto MTLCurveBasisCatmullRom = MTL::CurveBasisCatmullRom;
constexpr auto MTLCurveBasisLinear = MTL::CurveBasisLinear;
constexpr auto MTLCurveEndCapsDisk = MTL::CurveEndCapsDisk;
constexpr auto MTLCurveEndCapsNone = MTL::CurveEndCapsNone;
constexpr auto MTLCurveEndCapsSphere = MTL::CurveEndCapsSphere;
constexpr auto MTLCurveTypeFlat = MTL::CurveTypeFlat;
constexpr auto MTLCurveTypeRound = MTL::CurveTypeRound;
constexpr auto MTLDataTypeArray = MTL::DataTypeArray;
constexpr auto MTLDataTypeBFloat = MTL::DataTypeBFloat;
constexpr auto MTLDataTypeBFloat2 = MTL::DataTypeBFloat2;
constexpr auto MTLDataTypeBFloat3 = MTL::DataTypeBFloat3;
constexpr auto MTLDataTypeBFloat4 = MTL::DataTypeBFloat4;
constexpr auto MTLDataTypeBool = MTL::DataTypeBool;
constexpr auto MTLDataTypeBool2 = MTL::DataTypeBool2;
constexpr auto MTLDataTypeBool3 = MTL::DataTypeBool3;
constexpr auto MTLDataTypeBool4 = MTL::DataTypeBool4;
constexpr auto MTLDataTypeChar = MTL::DataTypeChar;
constexpr auto MTLDataTypeChar2 = MTL::DataTypeChar2;
constexpr auto MTLDataTypeChar3 = MTL::DataTypeChar3;
constexpr auto MTLDataTypeChar4 = MTL::DataTypeChar4;
constexpr auto MTLDataTypeComputePipeline = MTL::DataTypeComputePipeline;
constexpr auto MTLDataTypeDepthStencilState = MTL::DataTypeDepthStencilState;
constexpr auto MTLDataTypeFloat = MTL::DataTypeFloat;
constexpr auto MTLDataTypeFloat2 = MTL::DataTypeFloat2;
constexpr auto MTLDataTypeFloat2x2 = MTL::DataTypeFloat2x2;
constexpr auto MTLDataTypeFloat2x3 = MTL::DataTypeFloat2x3;
constexpr auto MTLDataTypeFloat2x4 = MTL::DataTypeFloat2x4;
constexpr auto MTLDataTypeFloat3 = MTL::DataTypeFloat3;
constexpr auto MTLDataTypeFloat3x2 = MTL::DataTypeFloat3x2;
constexpr auto MTLDataTypeFloat3x3 = MTL::DataTypeFloat3x3;
constexpr auto MTLDataTypeFloat3x4 = MTL::DataTypeFloat3x4;
constexpr auto MTLDataTypeFloat4 = MTL::DataTypeFloat4;
constexpr auto MTLDataTypeFloat4x2 = MTL::DataTypeFloat4x2;
constexpr auto MTLDataTypeFloat4x3 = MTL::DataTypeFloat4x3;
constexpr auto MTLDataTypeFloat4x4 = MTL::DataTypeFloat4x4;
constexpr auto MTLDataTypeHalf = MTL::DataTypeHalf;
constexpr auto MTLDataTypeHalf2 = MTL::DataTypeHalf2;
constexpr auto MTLDataTypeHalf2x2 = MTL::DataTypeHalf2x2;
constexpr auto MTLDataTypeHalf2x3 = MTL::DataTypeHalf2x3;
constexpr auto MTLDataTypeHalf2x4 = MTL::DataTypeHalf2x4;
constexpr auto MTLDataTypeHalf3 = MTL::DataTypeHalf3;
constexpr auto MTLDataTypeHalf3x2 = MTL::DataTypeHalf3x2;
constexpr auto MTLDataTypeHalf3x3 = MTL::DataTypeHalf3x3;
constexpr auto MTLDataTypeHalf3x4 = MTL::DataTypeHalf3x4;
constexpr auto MTLDataTypeHalf4 = MTL::DataTypeHalf4;
constexpr auto MTLDataTypeHalf4x2 = MTL::DataTypeHalf4x2;
constexpr auto MTLDataTypeHalf4x3 = MTL::DataTypeHalf4x3;
constexpr auto MTLDataTypeHalf4x4 = MTL::DataTypeHalf4x4;
constexpr auto MTLDataTypeIndirectCommandBuffer = MTL::DataTypeIndirectCommandBuffer;
constexpr auto MTLDataTypeInstanceAccelerationStructure = MTL::DataTypeInstanceAccelerationStructure;
constexpr auto MTLDataTypeInt = MTL::DataTypeInt;
constexpr auto MTLDataTypeInt2 = MTL::DataTypeInt2;
constexpr auto MTLDataTypeInt3 = MTL::DataTypeInt3;
constexpr auto MTLDataTypeInt4 = MTL::DataTypeInt4;
constexpr auto MTLDataTypeIntersectionFunctionTable = MTL::DataTypeIntersectionFunctionTable;
constexpr auto MTLDataTypeLong = MTL::DataTypeLong;
constexpr auto MTLDataTypeLong2 = MTL::DataTypeLong2;
constexpr auto MTLDataTypeLong3 = MTL::DataTypeLong3;
constexpr auto MTLDataTypeLong4 = MTL::DataTypeLong4;
constexpr auto MTLDataTypeNone = MTL::DataTypeNone;
constexpr auto MTLDataTypePointer = MTL::DataTypePointer;
constexpr auto MTLDataTypePrimitiveAccelerationStructure = MTL::DataTypePrimitiveAccelerationStructure;
constexpr auto MTLDataTypeR16Snorm = MTL::DataTypeR16Snorm;
constexpr auto MTLDataTypeR16Unorm = MTL::DataTypeR16Unorm;
constexpr auto MTLDataTypeR8Snorm = MTL::DataTypeR8Snorm;
constexpr auto MTLDataTypeR8Unorm = MTL::DataTypeR8Unorm;
constexpr auto MTLDataTypeRG11B10Float = MTL::DataTypeRG11B10Float;
constexpr auto MTLDataTypeRG16Snorm = MTL::DataTypeRG16Snorm;
constexpr auto MTLDataTypeRG16Unorm = MTL::DataTypeRG16Unorm;
constexpr auto MTLDataTypeRG8Snorm = MTL::DataTypeRG8Snorm;
constexpr auto MTLDataTypeRG8Unorm = MTL::DataTypeRG8Unorm;
constexpr auto MTLDataTypeRGB10A2Unorm = MTL::DataTypeRGB10A2Unorm;
constexpr auto MTLDataTypeRGB9E5Float = MTL::DataTypeRGB9E5Float;
constexpr auto MTLDataTypeRGBA16Snorm = MTL::DataTypeRGBA16Snorm;
constexpr auto MTLDataTypeRGBA16Unorm = MTL::DataTypeRGBA16Unorm;
constexpr auto MTLDataTypeRGBA8Snorm = MTL::DataTypeRGBA8Snorm;
constexpr auto MTLDataTypeRGBA8Unorm = MTL::DataTypeRGBA8Unorm;
constexpr auto MTLDataTypeRGBA8Unorm_sRGB = MTL::DataTypeRGBA8Unorm_sRGB;
constexpr auto MTLDataTypeRenderPipeline = MTL::DataTypeRenderPipeline;
constexpr auto MTLDataTypeSampler = MTL::DataTypeSampler;
constexpr auto MTLDataTypeShort = MTL::DataTypeShort;
constexpr auto MTLDataTypeShort2 = MTL::DataTypeShort2;
constexpr auto MTLDataTypeShort3 = MTL::DataTypeShort3;
constexpr auto MTLDataTypeShort4 = MTL::DataTypeShort4;
constexpr auto MTLDataTypeStruct = MTL::DataTypeStruct;
constexpr auto MTLDataTypeTensor = MTL::DataTypeTensor;
constexpr auto MTLDataTypeTexture = MTL::DataTypeTexture;
constexpr auto MTLDataTypeUChar = MTL::DataTypeUChar;
constexpr auto MTLDataTypeUChar2 = MTL::DataTypeUChar2;
constexpr auto MTLDataTypeUChar3 = MTL::DataTypeUChar3;
constexpr auto MTLDataTypeUChar4 = MTL::DataTypeUChar4;
constexpr auto MTLDataTypeUInt = MTL::DataTypeUInt;
constexpr auto MTLDataTypeUInt2 = MTL::DataTypeUInt2;
constexpr auto MTLDataTypeUInt3 = MTL::DataTypeUInt3;
constexpr auto MTLDataTypeUInt4 = MTL::DataTypeUInt4;
constexpr auto MTLDataTypeULong = MTL::DataTypeULong;
constexpr auto MTLDataTypeULong2 = MTL::DataTypeULong2;
constexpr auto MTLDataTypeULong3 = MTL::DataTypeULong3;
constexpr auto MTLDataTypeULong4 = MTL::DataTypeULong4;
constexpr auto MTLDataTypeUShort = MTL::DataTypeUShort;
constexpr auto MTLDataTypeUShort2 = MTL::DataTypeUShort2;
constexpr auto MTLDataTypeUShort3 = MTL::DataTypeUShort3;
constexpr auto MTLDataTypeUShort4 = MTL::DataTypeUShort4;
constexpr auto MTLDataTypeVisibleFunctionTable = MTL::DataTypeVisibleFunctionTable;
constexpr auto MTLDepthClipModeClamp = MTL::DepthClipModeClamp;
constexpr auto MTLDepthClipModeClip = MTL::DepthClipModeClip;
constexpr auto MTLDeviceErrorNone = MTL::DeviceErrorNone;
constexpr auto MTLDeviceErrorNotSupported = MTL::DeviceErrorNotSupported;
constexpr auto MTLDeviceLocationBuiltIn = MTL::DeviceLocationBuiltIn;
constexpr auto MTLDeviceLocationExternal = MTL::DeviceLocationExternal;
constexpr auto MTLDeviceLocationSlot = MTL::DeviceLocationSlot;
constexpr auto MTLDeviceLocationUnspecified = MTL::DeviceLocationUnspecified;
constexpr auto MTLDispatchTypeConcurrent = MTL::DispatchTypeConcurrent;
constexpr auto MTLDispatchTypeSerial = MTL::DispatchTypeSerial;
constexpr auto MTLDynamicLibraryErrorCompilationFailure = MTL::DynamicLibraryErrorCompilationFailure;
constexpr auto MTLDynamicLibraryErrorDependencyLoadFailure = MTL::DynamicLibraryErrorDependencyLoadFailure;
constexpr auto MTLDynamicLibraryErrorInvalidFile = MTL::DynamicLibraryErrorInvalidFile;
constexpr auto MTLDynamicLibraryErrorNone = MTL::DynamicLibraryErrorNone;
constexpr auto MTLDynamicLibraryErrorUnresolvedInstallName = MTL::DynamicLibraryErrorUnresolvedInstallName;
constexpr auto MTLDynamicLibraryErrorUnsupported = MTL::DynamicLibraryErrorUnsupported;
constexpr auto MTLFeatureSet_OSX_GPUFamily1_v1 = MTL::FeatureSet_OSX_GPUFamily1_v1;
constexpr auto MTLFeatureSet_OSX_GPUFamily1_v2 = MTL::FeatureSet_OSX_GPUFamily1_v2;
constexpr auto MTLFeatureSet_OSX_ReadWriteTextureTier2 = MTL::FeatureSet_OSX_ReadWriteTextureTier2;
constexpr auto MTLFeatureSet_TVOS_GPUFamily1_v1 = MTL::FeatureSet_TVOS_GPUFamily1_v1;
constexpr auto MTLFeatureSet_WatchOS_GPUFamily1_v1 = MTL::FeatureSet_WatchOS_GPUFamily1_v1;
constexpr auto MTLFeatureSet_WatchOS_GPUFamily2_v1 = MTL::FeatureSet_WatchOS_GPUFamily2_v1;
constexpr auto MTLFeatureSet_iOS_GPUFamily1_v1 = MTL::FeatureSet_iOS_GPUFamily1_v1;
constexpr auto MTLFeatureSet_iOS_GPUFamily1_v2 = MTL::FeatureSet_iOS_GPUFamily1_v2;
constexpr auto MTLFeatureSet_iOS_GPUFamily1_v3 = MTL::FeatureSet_iOS_GPUFamily1_v3;
constexpr auto MTLFeatureSet_iOS_GPUFamily1_v4 = MTL::FeatureSet_iOS_GPUFamily1_v4;
constexpr auto MTLFeatureSet_iOS_GPUFamily1_v5 = MTL::FeatureSet_iOS_GPUFamily1_v5;
constexpr auto MTLFeatureSet_iOS_GPUFamily2_v1 = MTL::FeatureSet_iOS_GPUFamily2_v1;
constexpr auto MTLFeatureSet_iOS_GPUFamily2_v2 = MTL::FeatureSet_iOS_GPUFamily2_v2;
constexpr auto MTLFeatureSet_iOS_GPUFamily2_v3 = MTL::FeatureSet_iOS_GPUFamily2_v3;
constexpr auto MTLFeatureSet_iOS_GPUFamily2_v4 = MTL::FeatureSet_iOS_GPUFamily2_v4;
constexpr auto MTLFeatureSet_iOS_GPUFamily2_v5 = MTL::FeatureSet_iOS_GPUFamily2_v5;
constexpr auto MTLFeatureSet_iOS_GPUFamily3_v1 = MTL::FeatureSet_iOS_GPUFamily3_v1;
constexpr auto MTLFeatureSet_iOS_GPUFamily3_v2 = MTL::FeatureSet_iOS_GPUFamily3_v2;
constexpr auto MTLFeatureSet_iOS_GPUFamily3_v3 = MTL::FeatureSet_iOS_GPUFamily3_v3;
constexpr auto MTLFeatureSet_iOS_GPUFamily3_v4 = MTL::FeatureSet_iOS_GPUFamily3_v4;
constexpr auto MTLFeatureSet_iOS_GPUFamily4_v1 = MTL::FeatureSet_iOS_GPUFamily4_v1;
constexpr auto MTLFeatureSet_iOS_GPUFamily4_v2 = MTL::FeatureSet_iOS_GPUFamily4_v2;
constexpr auto MTLFeatureSet_iOS_GPUFamily5_v1 = MTL::FeatureSet_iOS_GPUFamily5_v1;
constexpr auto MTLFeatureSet_macOS_GPUFamily1_v1 = MTL::FeatureSet_macOS_GPUFamily1_v1;
constexpr auto MTLFeatureSet_macOS_GPUFamily1_v2 = MTL::FeatureSet_macOS_GPUFamily1_v2;
constexpr auto MTLFeatureSet_macOS_GPUFamily1_v3 = MTL::FeatureSet_macOS_GPUFamily1_v3;
constexpr auto MTLFeatureSet_macOS_GPUFamily1_v4 = MTL::FeatureSet_macOS_GPUFamily1_v4;
constexpr auto MTLFeatureSet_macOS_GPUFamily2_v1 = MTL::FeatureSet_macOS_GPUFamily2_v1;
constexpr auto MTLFeatureSet_macOS_ReadWriteTextureTier2 = MTL::FeatureSet_macOS_ReadWriteTextureTier2;
constexpr auto MTLFeatureSet_tvOS_GPUFamily1_v1 = MTL::FeatureSet_tvOS_GPUFamily1_v1;
constexpr auto MTLFeatureSet_tvOS_GPUFamily1_v2 = MTL::FeatureSet_tvOS_GPUFamily1_v2;
constexpr auto MTLFeatureSet_tvOS_GPUFamily1_v3 = MTL::FeatureSet_tvOS_GPUFamily1_v3;
constexpr auto MTLFeatureSet_tvOS_GPUFamily1_v4 = MTL::FeatureSet_tvOS_GPUFamily1_v4;
constexpr auto MTLFeatureSet_tvOS_GPUFamily2_v1 = MTL::FeatureSet_tvOS_GPUFamily2_v1;
constexpr auto MTLFeatureSet_tvOS_GPUFamily2_v2 = MTL::FeatureSet_tvOS_GPUFamily2_v2;
constexpr auto MTLFeatureSet_watchOS_GPUFamily1_v1 = MTL::FeatureSet_watchOS_GPUFamily1_v1;
constexpr auto MTLFeatureSet_watchOS_GPUFamily2_v1 = MTL::FeatureSet_watchOS_GPUFamily2_v1;
constexpr auto MTLFunctionLogTypeValidation = MTL::FunctionLogTypeValidation;
constexpr auto MTLFunctionOptionCompileToBinary = MTL::FunctionOptionCompileToBinary;
constexpr auto MTLFunctionOptionFailOnBinaryArchiveMiss = MTL::FunctionOptionFailOnBinaryArchiveMiss;
constexpr auto MTLFunctionOptionNone = MTL::FunctionOptionNone;
constexpr auto MTLFunctionOptionPipelineIndependent = MTL::FunctionOptionPipelineIndependent;
constexpr auto MTLFunctionOptionStoreFunctionInMetalPipelinesScript = MTL::FunctionOptionStoreFunctionInMetalPipelinesScript;
constexpr auto MTLFunctionOptionStoreFunctionInMetalScript = MTL::FunctionOptionStoreFunctionInMetalScript;
constexpr auto MTLFunctionTypeFragment = MTL::FunctionTypeFragment;
constexpr auto MTLFunctionTypeIntersection = MTL::FunctionTypeIntersection;
constexpr auto MTLFunctionTypeKernel = MTL::FunctionTypeKernel;
constexpr auto MTLFunctionTypeMesh = MTL::FunctionTypeMesh;
constexpr auto MTLFunctionTypeObject = MTL::FunctionTypeObject;
constexpr auto MTLFunctionTypeVertex = MTL::FunctionTypeVertex;
constexpr auto MTLFunctionTypeVisible = MTL::FunctionTypeVisible;
constexpr auto MTLGPUFamilyApple1 = MTL::GPUFamilyApple1;
constexpr auto MTLGPUFamilyApple10 = MTL::GPUFamilyApple10;
constexpr auto MTLGPUFamilyApple2 = MTL::GPUFamilyApple2;
constexpr auto MTLGPUFamilyApple3 = MTL::GPUFamilyApple3;
constexpr auto MTLGPUFamilyApple4 = MTL::GPUFamilyApple4;
constexpr auto MTLGPUFamilyApple5 = MTL::GPUFamilyApple5;
constexpr auto MTLGPUFamilyApple6 = MTL::GPUFamilyApple6;
constexpr auto MTLGPUFamilyApple7 = MTL::GPUFamilyApple7;
constexpr auto MTLGPUFamilyApple8 = MTL::GPUFamilyApple8;
constexpr auto MTLGPUFamilyApple9 = MTL::GPUFamilyApple9;
constexpr auto MTLGPUFamilyCommon1 = MTL::GPUFamilyCommon1;
constexpr auto MTLGPUFamilyCommon2 = MTL::GPUFamilyCommon2;
constexpr auto MTLGPUFamilyCommon3 = MTL::GPUFamilyCommon3;
constexpr auto MTLGPUFamilyMac1 = MTL::GPUFamilyMac1;
constexpr auto MTLGPUFamilyMac2 = MTL::GPUFamilyMac2;
constexpr auto MTLGPUFamilyMacCatalyst1 = MTL::GPUFamilyMacCatalyst1;
constexpr auto MTLGPUFamilyMacCatalyst2 = MTL::GPUFamilyMacCatalyst2;
constexpr auto MTLGPUFamilyMetal3 = MTL::GPUFamilyMetal3;
constexpr auto MTLGPUFamilyMetal4 = MTL::GPUFamilyMetal4;
constexpr auto MTLHazardTrackingModeDefault = MTL::HazardTrackingModeDefault;
constexpr auto MTLHazardTrackingModeTracked = MTL::HazardTrackingModeTracked;
constexpr auto MTLHazardTrackingModeUntracked = MTL::HazardTrackingModeUntracked;
constexpr auto MTLHeapTypeAutomatic = MTL::HeapTypeAutomatic;
constexpr auto MTLHeapTypePlacement = MTL::HeapTypePlacement;
constexpr auto MTLHeapTypeSparse = MTL::HeapTypeSparse;
constexpr auto MTLIOCommandQueueTypeConcurrent = MTL::IOCommandQueueTypeConcurrent;
constexpr auto MTLIOCommandQueueTypeSerial = MTL::IOCommandQueueTypeSerial;
constexpr auto MTLIOCompressionMethodLZ4 = MTL::IOCompressionMethodLZ4;
constexpr auto MTLIOCompressionMethodLZBitmap = MTL::IOCompressionMethodLZBitmap;
constexpr auto MTLIOCompressionMethodLZFSE = MTL::IOCompressionMethodLZFSE;
constexpr auto MTLIOCompressionMethodLZMA = MTL::IOCompressionMethodLZMA;
constexpr auto MTLIOCompressionMethodZlib = MTL::IOCompressionMethodZlib;
constexpr auto MTLIOCompressionStatusComplete = MTL::IOCompressionStatusComplete;
constexpr auto MTLIOCompressionStatusError = MTL::IOCompressionStatusError;
constexpr auto MTLIOErrorInternal = MTL::IOErrorInternal;
constexpr auto MTLIOErrorURLInvalid = MTL::IOErrorURLInvalid;
constexpr auto MTLIOPriorityHigh = MTL::IOPriorityHigh;
constexpr auto MTLIOPriorityLow = MTL::IOPriorityLow;
constexpr auto MTLIOPriorityNormal = MTL::IOPriorityNormal;
constexpr auto MTLIOStatusCancelled = MTL::IOStatusCancelled;
constexpr auto MTLIOStatusComplete = MTL::IOStatusComplete;
constexpr auto MTLIOStatusError = MTL::IOStatusError;
constexpr auto MTLIOStatusPending = MTL::IOStatusPending;
constexpr auto MTLIndexTypeUInt16 = MTL::IndexTypeUInt16;
constexpr auto MTLIndexTypeUInt32 = MTL::IndexTypeUInt32;
constexpr auto MTLIndirectCommandTypeConcurrentDispatch = MTL::IndirectCommandTypeConcurrentDispatch;
constexpr auto MTLIndirectCommandTypeConcurrentDispatchThreads = MTL::IndirectCommandTypeConcurrentDispatchThreads;
constexpr auto MTLIndirectCommandTypeDraw = MTL::IndirectCommandTypeDraw;
constexpr auto MTLIndirectCommandTypeDrawIndexed = MTL::IndirectCommandTypeDrawIndexed;
constexpr auto MTLIndirectCommandTypeDrawIndexedPatches = MTL::IndirectCommandTypeDrawIndexedPatches;
constexpr auto MTLIndirectCommandTypeDrawMeshThreadgroups = MTL::IndirectCommandTypeDrawMeshThreadgroups;
constexpr auto MTLIndirectCommandTypeDrawMeshThreads = MTL::IndirectCommandTypeDrawMeshThreads;
constexpr auto MTLIndirectCommandTypeDrawPatches = MTL::IndirectCommandTypeDrawPatches;
constexpr auto MTLIntersectionFunctionSignatureCurveData = MTL::IntersectionFunctionSignatureCurveData;
constexpr auto MTLIntersectionFunctionSignatureExtendedLimits = MTL::IntersectionFunctionSignatureExtendedLimits;
constexpr auto MTLIntersectionFunctionSignatureInstanceMotion = MTL::IntersectionFunctionSignatureInstanceMotion;
constexpr auto MTLIntersectionFunctionSignatureInstancing = MTL::IntersectionFunctionSignatureInstancing;
constexpr auto MTLIntersectionFunctionSignatureIntersectionFunctionBuffer = MTL::IntersectionFunctionSignatureIntersectionFunctionBuffer;
constexpr auto MTLIntersectionFunctionSignatureMaxLevels = MTL::IntersectionFunctionSignatureMaxLevels;
constexpr auto MTLIntersectionFunctionSignatureNone = MTL::IntersectionFunctionSignatureNone;
constexpr auto MTLIntersectionFunctionSignaturePrimitiveMotion = MTL::IntersectionFunctionSignaturePrimitiveMotion;
constexpr auto MTLIntersectionFunctionSignatureTriangleData = MTL::IntersectionFunctionSignatureTriangleData;
constexpr auto MTLIntersectionFunctionSignatureUserData = MTL::IntersectionFunctionSignatureUserData;
constexpr auto MTLIntersectionFunctionSignatureWorldSpaceData = MTL::IntersectionFunctionSignatureWorldSpaceData;
constexpr auto MTLLanguageVersion1_0 = MTL::LanguageVersion1_0;
constexpr auto MTLLanguageVersion1_1 = MTL::LanguageVersion1_1;
constexpr auto MTLLanguageVersion1_2 = MTL::LanguageVersion1_2;
constexpr auto MTLLanguageVersion2_0 = MTL::LanguageVersion2_0;
constexpr auto MTLLanguageVersion2_1 = MTL::LanguageVersion2_1;
constexpr auto MTLLanguageVersion2_2 = MTL::LanguageVersion2_2;
constexpr auto MTLLanguageVersion2_3 = MTL::LanguageVersion2_3;
constexpr auto MTLLanguageVersion2_4 = MTL::LanguageVersion2_4;
constexpr auto MTLLanguageVersion3_0 = MTL::LanguageVersion3_0;
constexpr auto MTLLanguageVersion3_1 = MTL::LanguageVersion3_1;
constexpr auto MTLLanguageVersion3_2 = MTL::LanguageVersion3_2;
constexpr auto MTLLanguageVersion4_0 = MTL::LanguageVersion4_0;
constexpr auto MTLLibraryErrorCompileFailure = MTL::LibraryErrorCompileFailure;
constexpr auto MTLLibraryErrorCompileWarning = MTL::LibraryErrorCompileWarning;
constexpr auto MTLLibraryErrorFileNotFound = MTL::LibraryErrorFileNotFound;
constexpr auto MTLLibraryErrorFunctionNotFound = MTL::LibraryErrorFunctionNotFound;
constexpr auto MTLLibraryErrorInternal = MTL::LibraryErrorInternal;
constexpr auto MTLLibraryErrorUnsupported = MTL::LibraryErrorUnsupported;
constexpr auto MTLLibraryOptimizationLevelDefault = MTL::LibraryOptimizationLevelDefault;
constexpr auto MTLLibraryOptimizationLevelSize = MTL::LibraryOptimizationLevelSize;
constexpr auto MTLLibraryTypeDynamic = MTL::LibraryTypeDynamic;
constexpr auto MTLLibraryTypeExecutable = MTL::LibraryTypeExecutable;
constexpr auto MTLLoadActionClear = MTL::LoadActionClear;
constexpr auto MTLLoadActionDontCare = MTL::LoadActionDontCare;
constexpr auto MTLLoadActionLoad = MTL::LoadActionLoad;
constexpr auto MTLLogLevelDebug = MTL::LogLevelDebug;
constexpr auto MTLLogLevelError = MTL::LogLevelError;
constexpr auto MTLLogLevelFault = MTL::LogLevelFault;
constexpr auto MTLLogLevelInfo = MTL::LogLevelInfo;
constexpr auto MTLLogLevelNotice = MTL::LogLevelNotice;
constexpr auto MTLLogLevelUndefined = MTL::LogLevelUndefined;
constexpr auto MTLLogStateErrorInvalid = MTL::LogStateErrorInvalid;
constexpr auto MTLLogStateErrorInvalidSize = MTL::LogStateErrorInvalidSize;
constexpr auto MTLMathFloatingPointFunctionsFast = MTL::MathFloatingPointFunctionsFast;
constexpr auto MTLMathFloatingPointFunctionsPrecise = MTL::MathFloatingPointFunctionsPrecise;
constexpr auto MTLMathModeFast = MTL::MathModeFast;
constexpr auto MTLMathModeRelaxed = MTL::MathModeRelaxed;
constexpr auto MTLMathModeSafe = MTL::MathModeSafe;
constexpr auto MTLMatrixLayoutColumnMajor = MTL::MatrixLayoutColumnMajor;
constexpr auto MTLMatrixLayoutRowMajor = MTL::MatrixLayoutRowMajor;
constexpr auto MTLMotionBorderModeClamp = MTL::MotionBorderModeClamp;
constexpr auto MTLMotionBorderModeVanish = MTL::MotionBorderModeVanish;
constexpr auto MTLMultisampleDepthResolveFilterMax = MTL::MultisampleDepthResolveFilterMax;
constexpr auto MTLMultisampleDepthResolveFilterMin = MTL::MultisampleDepthResolveFilterMin;
constexpr auto MTLMultisampleDepthResolveFilterSample0 = MTL::MultisampleDepthResolveFilterSample0;
constexpr auto MTLMultisampleStencilResolveFilterDepthResolvedSample = MTL::MultisampleStencilResolveFilterDepthResolvedSample;
constexpr auto MTLMultisampleStencilResolveFilterSample0 = MTL::MultisampleStencilResolveFilterSample0;
constexpr auto MTLMutabilityDefault = MTL::MutabilityDefault;
constexpr auto MTLMutabilityImmutable = MTL::MutabilityImmutable;
constexpr auto MTLMutabilityMutable = MTL::MutabilityMutable;
constexpr auto MTLPatchTypeNone = MTL::PatchTypeNone;
constexpr auto MTLPatchTypeQuad = MTL::PatchTypeQuad;
constexpr auto MTLPatchTypeTriangle = MTL::PatchTypeTriangle;
constexpr auto MTLPipelineOptionArgumentInfo = MTL::PipelineOptionArgumentInfo;
constexpr auto MTLPipelineOptionBindingInfo = MTL::PipelineOptionBindingInfo;
constexpr auto MTLPipelineOptionBufferTypeInfo = MTL::PipelineOptionBufferTypeInfo;
constexpr auto MTLPipelineOptionFailOnBinaryArchiveMiss = MTL::PipelineOptionFailOnBinaryArchiveMiss;
constexpr auto MTLPipelineOptionNone = MTL::PipelineOptionNone;
constexpr auto MTLPixelFormatA1BGR5Unorm = MTL::PixelFormatA1BGR5Unorm;
constexpr auto MTLPixelFormatA8Unorm = MTL::PixelFormatA8Unorm;
constexpr auto MTLPixelFormatABGR4Unorm = MTL::PixelFormatABGR4Unorm;
constexpr auto MTLPixelFormatASTC_10x10_HDR = MTL::PixelFormatASTC_10x10_HDR;
constexpr auto MTLPixelFormatASTC_10x10_LDR = MTL::PixelFormatASTC_10x10_LDR;
constexpr auto MTLPixelFormatASTC_10x10_sRGB = MTL::PixelFormatASTC_10x10_sRGB;
constexpr auto MTLPixelFormatASTC_10x5_HDR = MTL::PixelFormatASTC_10x5_HDR;
constexpr auto MTLPixelFormatASTC_10x5_LDR = MTL::PixelFormatASTC_10x5_LDR;
constexpr auto MTLPixelFormatASTC_10x5_sRGB = MTL::PixelFormatASTC_10x5_sRGB;
constexpr auto MTLPixelFormatASTC_10x6_HDR = MTL::PixelFormatASTC_10x6_HDR;
constexpr auto MTLPixelFormatASTC_10x6_LDR = MTL::PixelFormatASTC_10x6_LDR;
constexpr auto MTLPixelFormatASTC_10x6_sRGB = MTL::PixelFormatASTC_10x6_sRGB;
constexpr auto MTLPixelFormatASTC_10x8_HDR = MTL::PixelFormatASTC_10x8_HDR;
constexpr auto MTLPixelFormatASTC_10x8_LDR = MTL::PixelFormatASTC_10x8_LDR;
constexpr auto MTLPixelFormatASTC_10x8_sRGB = MTL::PixelFormatASTC_10x8_sRGB;
constexpr auto MTLPixelFormatASTC_12x10_HDR = MTL::PixelFormatASTC_12x10_HDR;
constexpr auto MTLPixelFormatASTC_12x10_LDR = MTL::PixelFormatASTC_12x10_LDR;
constexpr auto MTLPixelFormatASTC_12x10_sRGB = MTL::PixelFormatASTC_12x10_sRGB;
constexpr auto MTLPixelFormatASTC_12x12_HDR = MTL::PixelFormatASTC_12x12_HDR;
constexpr auto MTLPixelFormatASTC_12x12_LDR = MTL::PixelFormatASTC_12x12_LDR;
constexpr auto MTLPixelFormatASTC_12x12_sRGB = MTL::PixelFormatASTC_12x12_sRGB;
constexpr auto MTLPixelFormatASTC_4x4_HDR = MTL::PixelFormatASTC_4x4_HDR;
constexpr auto MTLPixelFormatASTC_4x4_LDR = MTL::PixelFormatASTC_4x4_LDR;
constexpr auto MTLPixelFormatASTC_4x4_sRGB = MTL::PixelFormatASTC_4x4_sRGB;
constexpr auto MTLPixelFormatASTC_5x4_HDR = MTL::PixelFormatASTC_5x4_HDR;
constexpr auto MTLPixelFormatASTC_5x4_LDR = MTL::PixelFormatASTC_5x4_LDR;
constexpr auto MTLPixelFormatASTC_5x4_sRGB = MTL::PixelFormatASTC_5x4_sRGB;
constexpr auto MTLPixelFormatASTC_5x5_HDR = MTL::PixelFormatASTC_5x5_HDR;
constexpr auto MTLPixelFormatASTC_5x5_LDR = MTL::PixelFormatASTC_5x5_LDR;
constexpr auto MTLPixelFormatASTC_5x5_sRGB = MTL::PixelFormatASTC_5x5_sRGB;
constexpr auto MTLPixelFormatASTC_6x5_HDR = MTL::PixelFormatASTC_6x5_HDR;
constexpr auto MTLPixelFormatASTC_6x5_LDR = MTL::PixelFormatASTC_6x5_LDR;
constexpr auto MTLPixelFormatASTC_6x5_sRGB = MTL::PixelFormatASTC_6x5_sRGB;
constexpr auto MTLPixelFormatASTC_6x6_HDR = MTL::PixelFormatASTC_6x6_HDR;
constexpr auto MTLPixelFormatASTC_6x6_LDR = MTL::PixelFormatASTC_6x6_LDR;
constexpr auto MTLPixelFormatASTC_6x6_sRGB = MTL::PixelFormatASTC_6x6_sRGB;
constexpr auto MTLPixelFormatASTC_8x5_HDR = MTL::PixelFormatASTC_8x5_HDR;
constexpr auto MTLPixelFormatASTC_8x5_LDR = MTL::PixelFormatASTC_8x5_LDR;
constexpr auto MTLPixelFormatASTC_8x5_sRGB = MTL::PixelFormatASTC_8x5_sRGB;
constexpr auto MTLPixelFormatASTC_8x6_HDR = MTL::PixelFormatASTC_8x6_HDR;
constexpr auto MTLPixelFormatASTC_8x6_LDR = MTL::PixelFormatASTC_8x6_LDR;
constexpr auto MTLPixelFormatASTC_8x6_sRGB = MTL::PixelFormatASTC_8x6_sRGB;
constexpr auto MTLPixelFormatASTC_8x8_HDR = MTL::PixelFormatASTC_8x8_HDR;
constexpr auto MTLPixelFormatASTC_8x8_LDR = MTL::PixelFormatASTC_8x8_LDR;
constexpr auto MTLPixelFormatASTC_8x8_sRGB = MTL::PixelFormatASTC_8x8_sRGB;
constexpr auto MTLPixelFormatB5G6R5Unorm = MTL::PixelFormatB5G6R5Unorm;
constexpr auto MTLPixelFormatBC1_RGBA = MTL::PixelFormatBC1_RGBA;
constexpr auto MTLPixelFormatBC1_RGBA_sRGB = MTL::PixelFormatBC1_RGBA_sRGB;
constexpr auto MTLPixelFormatBC2_RGBA = MTL::PixelFormatBC2_RGBA;
constexpr auto MTLPixelFormatBC2_RGBA_sRGB = MTL::PixelFormatBC2_RGBA_sRGB;
constexpr auto MTLPixelFormatBC3_RGBA = MTL::PixelFormatBC3_RGBA;
constexpr auto MTLPixelFormatBC3_RGBA_sRGB = MTL::PixelFormatBC3_RGBA_sRGB;
constexpr auto MTLPixelFormatBC4_RSnorm = MTL::PixelFormatBC4_RSnorm;
constexpr auto MTLPixelFormatBC4_RUnorm = MTL::PixelFormatBC4_RUnorm;
constexpr auto MTLPixelFormatBC5_RGSnorm = MTL::PixelFormatBC5_RGSnorm;
constexpr auto MTLPixelFormatBC5_RGUnorm = MTL::PixelFormatBC5_RGUnorm;
constexpr auto MTLPixelFormatBC6H_RGBFloat = MTL::PixelFormatBC6H_RGBFloat;
constexpr auto MTLPixelFormatBC6H_RGBUfloat = MTL::PixelFormatBC6H_RGBUfloat;
constexpr auto MTLPixelFormatBC7_RGBAUnorm = MTL::PixelFormatBC7_RGBAUnorm;
constexpr auto MTLPixelFormatBC7_RGBAUnorm_sRGB = MTL::PixelFormatBC7_RGBAUnorm_sRGB;
constexpr auto MTLPixelFormatBGR10A2Unorm = MTL::PixelFormatBGR10A2Unorm;
constexpr auto MTLPixelFormatBGR10_XR = MTL::PixelFormatBGR10_XR;
constexpr auto MTLPixelFormatBGR10_XR_sRGB = MTL::PixelFormatBGR10_XR_sRGB;
constexpr auto MTLPixelFormatBGR5A1Unorm = MTL::PixelFormatBGR5A1Unorm;
constexpr auto MTLPixelFormatBGRA10_XR = MTL::PixelFormatBGRA10_XR;
constexpr auto MTLPixelFormatBGRA10_XR_sRGB = MTL::PixelFormatBGRA10_XR_sRGB;
constexpr auto MTLPixelFormatBGRA8Unorm = MTL::PixelFormatBGRA8Unorm;
constexpr auto MTLPixelFormatBGRA8Unorm_sRGB = MTL::PixelFormatBGRA8Unorm_sRGB;
constexpr auto MTLPixelFormatBGRG422 = MTL::PixelFormatBGRG422;
constexpr auto MTLPixelFormatDepth16Unorm = MTL::PixelFormatDepth16Unorm;
constexpr auto MTLPixelFormatDepth24Unorm_Stencil8 = MTL::PixelFormatDepth24Unorm_Stencil8;
constexpr auto MTLPixelFormatDepth32Float = MTL::PixelFormatDepth32Float;
constexpr auto MTLPixelFormatDepth32Float_Stencil8 = MTL::PixelFormatDepth32Float_Stencil8;
constexpr auto MTLPixelFormatEAC_R11Snorm = MTL::PixelFormatEAC_R11Snorm;
constexpr auto MTLPixelFormatEAC_R11Unorm = MTL::PixelFormatEAC_R11Unorm;
constexpr auto MTLPixelFormatEAC_RG11Snorm = MTL::PixelFormatEAC_RG11Snorm;
constexpr auto MTLPixelFormatEAC_RG11Unorm = MTL::PixelFormatEAC_RG11Unorm;
constexpr auto MTLPixelFormatEAC_RGBA8 = MTL::PixelFormatEAC_RGBA8;
constexpr auto MTLPixelFormatEAC_RGBA8_sRGB = MTL::PixelFormatEAC_RGBA8_sRGB;
constexpr auto MTLPixelFormatETC2_RGB8 = MTL::PixelFormatETC2_RGB8;
constexpr auto MTLPixelFormatETC2_RGB8A1 = MTL::PixelFormatETC2_RGB8A1;
constexpr auto MTLPixelFormatETC2_RGB8A1_sRGB = MTL::PixelFormatETC2_RGB8A1_sRGB;
constexpr auto MTLPixelFormatETC2_RGB8_sRGB = MTL::PixelFormatETC2_RGB8_sRGB;
constexpr auto MTLPixelFormatGBGR422 = MTL::PixelFormatGBGR422;
constexpr auto MTLPixelFormatInvalid = MTL::PixelFormatInvalid;
constexpr auto MTLPixelFormatPVRTC_RGBA_2BPP = MTL::PixelFormatPVRTC_RGBA_2BPP;
constexpr auto MTLPixelFormatPVRTC_RGBA_2BPP_sRGB = MTL::PixelFormatPVRTC_RGBA_2BPP_sRGB;
constexpr auto MTLPixelFormatPVRTC_RGBA_4BPP = MTL::PixelFormatPVRTC_RGBA_4BPP;
constexpr auto MTLPixelFormatPVRTC_RGBA_4BPP_sRGB = MTL::PixelFormatPVRTC_RGBA_4BPP_sRGB;
constexpr auto MTLPixelFormatPVRTC_RGB_2BPP = MTL::PixelFormatPVRTC_RGB_2BPP;
constexpr auto MTLPixelFormatPVRTC_RGB_2BPP_sRGB = MTL::PixelFormatPVRTC_RGB_2BPP_sRGB;
constexpr auto MTLPixelFormatPVRTC_RGB_4BPP = MTL::PixelFormatPVRTC_RGB_4BPP;
constexpr auto MTLPixelFormatPVRTC_RGB_4BPP_sRGB = MTL::PixelFormatPVRTC_RGB_4BPP_sRGB;
constexpr auto MTLPixelFormatR16Float = MTL::PixelFormatR16Float;
constexpr auto MTLPixelFormatR16Sint = MTL::PixelFormatR16Sint;
constexpr auto MTLPixelFormatR16Snorm = MTL::PixelFormatR16Snorm;
constexpr auto MTLPixelFormatR16Uint = MTL::PixelFormatR16Uint;
constexpr auto MTLPixelFormatR16Unorm = MTL::PixelFormatR16Unorm;
constexpr auto MTLPixelFormatR32Float = MTL::PixelFormatR32Float;
constexpr auto MTLPixelFormatR32Sint = MTL::PixelFormatR32Sint;
constexpr auto MTLPixelFormatR32Uint = MTL::PixelFormatR32Uint;
constexpr auto MTLPixelFormatR8Sint = MTL::PixelFormatR8Sint;
constexpr auto MTLPixelFormatR8Snorm = MTL::PixelFormatR8Snorm;
constexpr auto MTLPixelFormatR8Uint = MTL::PixelFormatR8Uint;
constexpr auto MTLPixelFormatR8Unorm = MTL::PixelFormatR8Unorm;
constexpr auto MTLPixelFormatR8Unorm_sRGB = MTL::PixelFormatR8Unorm_sRGB;
constexpr auto MTLPixelFormatRG11B10Float = MTL::PixelFormatRG11B10Float;
constexpr auto MTLPixelFormatRG16Float = MTL::PixelFormatRG16Float;
constexpr auto MTLPixelFormatRG16Sint = MTL::PixelFormatRG16Sint;
constexpr auto MTLPixelFormatRG16Snorm = MTL::PixelFormatRG16Snorm;
constexpr auto MTLPixelFormatRG16Uint = MTL::PixelFormatRG16Uint;
constexpr auto MTLPixelFormatRG16Unorm = MTL::PixelFormatRG16Unorm;
constexpr auto MTLPixelFormatRG32Float = MTL::PixelFormatRG32Float;
constexpr auto MTLPixelFormatRG32Sint = MTL::PixelFormatRG32Sint;
constexpr auto MTLPixelFormatRG32Uint = MTL::PixelFormatRG32Uint;
constexpr auto MTLPixelFormatRG8Sint = MTL::PixelFormatRG8Sint;
constexpr auto MTLPixelFormatRG8Snorm = MTL::PixelFormatRG8Snorm;
constexpr auto MTLPixelFormatRG8Uint = MTL::PixelFormatRG8Uint;
constexpr auto MTLPixelFormatRG8Unorm = MTL::PixelFormatRG8Unorm;
constexpr auto MTLPixelFormatRG8Unorm_sRGB = MTL::PixelFormatRG8Unorm_sRGB;
constexpr auto MTLPixelFormatRGB10A2Uint = MTL::PixelFormatRGB10A2Uint;
constexpr auto MTLPixelFormatRGB10A2Unorm = MTL::PixelFormatRGB10A2Unorm;
constexpr auto MTLPixelFormatRGB9E5Float = MTL::PixelFormatRGB9E5Float;
constexpr auto MTLPixelFormatRGBA16Float = MTL::PixelFormatRGBA16Float;
constexpr auto MTLPixelFormatRGBA16Sint = MTL::PixelFormatRGBA16Sint;
constexpr auto MTLPixelFormatRGBA16Snorm = MTL::PixelFormatRGBA16Snorm;
constexpr auto MTLPixelFormatRGBA16Uint = MTL::PixelFormatRGBA16Uint;
constexpr auto MTLPixelFormatRGBA16Unorm = MTL::PixelFormatRGBA16Unorm;
constexpr auto MTLPixelFormatRGBA32Float = MTL::PixelFormatRGBA32Float;
constexpr auto MTLPixelFormatRGBA32Sint = MTL::PixelFormatRGBA32Sint;
constexpr auto MTLPixelFormatRGBA32Uint = MTL::PixelFormatRGBA32Uint;
constexpr auto MTLPixelFormatRGBA8Sint = MTL::PixelFormatRGBA8Sint;
constexpr auto MTLPixelFormatRGBA8Snorm = MTL::PixelFormatRGBA8Snorm;
constexpr auto MTLPixelFormatRGBA8Uint = MTL::PixelFormatRGBA8Uint;
constexpr auto MTLPixelFormatRGBA8Unorm = MTL::PixelFormatRGBA8Unorm;
constexpr auto MTLPixelFormatRGBA8Unorm_sRGB = MTL::PixelFormatRGBA8Unorm_sRGB;
constexpr auto MTLPixelFormatStencil8 = MTL::PixelFormatStencil8;
constexpr auto MTLPixelFormatUnspecialized = MTL::PixelFormatUnspecialized;
constexpr auto MTLPixelFormatX24_Stencil8 = MTL::PixelFormatX24_Stencil8;
constexpr auto MTLPixelFormatX32_Stencil8 = MTL::PixelFormatX32_Stencil8;
constexpr auto MTLPrimitiveTopologyClassLine = MTL::PrimitiveTopologyClassLine;
constexpr auto MTLPrimitiveTopologyClassPoint = MTL::PrimitiveTopologyClassPoint;
constexpr auto MTLPrimitiveTopologyClassTriangle = MTL::PrimitiveTopologyClassTriangle;
constexpr auto MTLPrimitiveTopologyClassUnspecified = MTL::PrimitiveTopologyClassUnspecified;
constexpr auto MTLPrimitiveTypeLine = MTL::PrimitiveTypeLine;
constexpr auto MTLPrimitiveTypeLineStrip = MTL::PrimitiveTypeLineStrip;
constexpr auto MTLPrimitiveTypePoint = MTL::PrimitiveTypePoint;
constexpr auto MTLPrimitiveTypeTriangle = MTL::PrimitiveTypeTriangle;
constexpr auto MTLPrimitiveTypeTriangleStrip = MTL::PrimitiveTypeTriangleStrip;
constexpr auto MTLPurgeableStateEmpty = MTL::PurgeableStateEmpty;
constexpr auto MTLPurgeableStateKeepCurrent = MTL::PurgeableStateKeepCurrent;
constexpr auto MTLPurgeableStateNonVolatile = MTL::PurgeableStateNonVolatile;
constexpr auto MTLPurgeableStateVolatile = MTL::PurgeableStateVolatile;
constexpr auto MTLReadWriteTextureTier1 = MTL::ReadWriteTextureTier1;
constexpr auto MTLReadWriteTextureTier2 = MTL::ReadWriteTextureTier2;
constexpr auto MTLReadWriteTextureTierNone = MTL::ReadWriteTextureTierNone;
constexpr auto MTLRenderStageFragment = MTL::RenderStageFragment;
constexpr auto MTLRenderStageMesh = MTL::RenderStageMesh;
constexpr auto MTLRenderStageObject = MTL::RenderStageObject;
constexpr auto MTLRenderStageTile = MTL::RenderStageTile;
constexpr auto MTLRenderStageVertex = MTL::RenderStageVertex;
constexpr auto MTLResourceCPUCacheModeDefaultCache = MTL::ResourceCPUCacheModeDefaultCache;
constexpr auto MTLResourceCPUCacheModeWriteCombined = MTL::ResourceCPUCacheModeWriteCombined;
constexpr auto MTLResourceHazardTrackingModeDefault = MTL::ResourceHazardTrackingModeDefault;
constexpr auto MTLResourceHazardTrackingModeTracked = MTL::ResourceHazardTrackingModeTracked;
constexpr auto MTLResourceHazardTrackingModeUntracked = MTL::ResourceHazardTrackingModeUntracked;
constexpr auto MTLResourceStorageModeManaged = MTL::ResourceStorageModeManaged;
constexpr auto MTLResourceStorageModeMemoryless = MTL::ResourceStorageModeMemoryless;
constexpr auto MTLResourceStorageModePrivate = MTL::ResourceStorageModePrivate;
constexpr auto MTLResourceStorageModeShared = MTL::ResourceStorageModeShared;
constexpr auto MTLResourceUsageRead = MTL::ResourceUsageRead;
constexpr auto MTLResourceUsageSample = MTL::ResourceUsageSample;
constexpr auto MTLResourceUsageWrite = MTL::ResourceUsageWrite;
constexpr auto MTLSamplerAddressModeClampToBorderColor = MTL::SamplerAddressModeClampToBorderColor;
constexpr auto MTLSamplerAddressModeClampToEdge = MTL::SamplerAddressModeClampToEdge;
constexpr auto MTLSamplerAddressModeClampToZero = MTL::SamplerAddressModeClampToZero;
constexpr auto MTLSamplerAddressModeMirrorClampToEdge = MTL::SamplerAddressModeMirrorClampToEdge;
constexpr auto MTLSamplerAddressModeMirrorRepeat = MTL::SamplerAddressModeMirrorRepeat;
constexpr auto MTLSamplerAddressModeRepeat = MTL::SamplerAddressModeRepeat;
constexpr auto MTLSamplerBorderColorOpaqueBlack = MTL::SamplerBorderColorOpaqueBlack;
constexpr auto MTLSamplerBorderColorOpaqueWhite = MTL::SamplerBorderColorOpaqueWhite;
constexpr auto MTLSamplerBorderColorTransparentBlack = MTL::SamplerBorderColorTransparentBlack;
constexpr auto MTLSamplerMinMagFilterLinear = MTL::SamplerMinMagFilterLinear;
constexpr auto MTLSamplerMinMagFilterNearest = MTL::SamplerMinMagFilterNearest;
constexpr auto MTLSamplerMipFilterLinear = MTL::SamplerMipFilterLinear;
constexpr auto MTLSamplerMipFilterNearest = MTL::SamplerMipFilterNearest;
constexpr auto MTLSamplerMipFilterNotMipmapped = MTL::SamplerMipFilterNotMipmapped;
constexpr auto MTLSamplerReductionModeMaximum = MTL::SamplerReductionModeMaximum;
constexpr auto MTLSamplerReductionModeMinimum = MTL::SamplerReductionModeMinimum;
constexpr auto MTLSamplerReductionModeWeightedAverage = MTL::SamplerReductionModeWeightedAverage;
constexpr auto MTLShaderValidationDefault = MTL::ShaderValidationDefault;
constexpr auto MTLShaderValidationDisabled = MTL::ShaderValidationDisabled;
constexpr auto MTLShaderValidationEnabled = MTL::ShaderValidationEnabled;
constexpr auto MTLSparsePageSize16 = MTL::SparsePageSize16;
constexpr auto MTLSparsePageSize256 = MTL::SparsePageSize256;
constexpr auto MTLSparsePageSize64 = MTL::SparsePageSize64;
constexpr auto MTLSparseTextureMappingModeMap = MTL::SparseTextureMappingModeMap;
constexpr auto MTLSparseTextureMappingModeUnmap = MTL::SparseTextureMappingModeUnmap;
constexpr auto MTLSparseTextureRegionAlignmentModeInward = MTL::SparseTextureRegionAlignmentModeInward;
constexpr auto MTLSparseTextureRegionAlignmentModeOutward = MTL::SparseTextureRegionAlignmentModeOutward;
constexpr auto MTLStageAccelerationStructure = MTL::StageAccelerationStructure;
constexpr auto MTLStageAll = MTL::StageAll;
constexpr auto MTLStageBlit = MTL::StageBlit;
constexpr auto MTLStageDispatch = MTL::StageDispatch;
constexpr auto MTLStageFragment = MTL::StageFragment;
constexpr auto MTLStageMachineLearning = MTL::StageMachineLearning;
constexpr auto MTLStageMesh = MTL::StageMesh;
constexpr auto MTLStageObject = MTL::StageObject;
constexpr auto MTLStageResourceState = MTL::StageResourceState;
constexpr auto MTLStageTile = MTL::StageTile;
constexpr auto MTLStageVertex = MTL::StageVertex;
constexpr auto MTLStencilOperationDecrementClamp = MTL::StencilOperationDecrementClamp;
constexpr auto MTLStencilOperationDecrementWrap = MTL::StencilOperationDecrementWrap;
constexpr auto MTLStencilOperationIncrementClamp = MTL::StencilOperationIncrementClamp;
constexpr auto MTLStencilOperationIncrementWrap = MTL::StencilOperationIncrementWrap;
constexpr auto MTLStencilOperationInvert = MTL::StencilOperationInvert;
constexpr auto MTLStencilOperationKeep = MTL::StencilOperationKeep;
constexpr auto MTLStencilOperationReplace = MTL::StencilOperationReplace;
constexpr auto MTLStencilOperationZero = MTL::StencilOperationZero;
constexpr auto MTLStepFunctionConstant = MTL::StepFunctionConstant;
constexpr auto MTLStepFunctionPerInstance = MTL::StepFunctionPerInstance;
constexpr auto MTLStepFunctionPerPatch = MTL::StepFunctionPerPatch;
constexpr auto MTLStepFunctionPerPatchControlPoint = MTL::StepFunctionPerPatchControlPoint;
constexpr auto MTLStepFunctionPerVertex = MTL::StepFunctionPerVertex;
constexpr auto MTLStepFunctionThreadPositionInGridX = MTL::StepFunctionThreadPositionInGridX;
constexpr auto MTLStepFunctionThreadPositionInGridXIndexed = MTL::StepFunctionThreadPositionInGridXIndexed;
constexpr auto MTLStepFunctionThreadPositionInGridY = MTL::StepFunctionThreadPositionInGridY;
constexpr auto MTLStepFunctionThreadPositionInGridYIndexed = MTL::StepFunctionThreadPositionInGridYIndexed;
constexpr auto MTLStitchedLibraryOptionFailOnBinaryArchiveMiss = MTL::StitchedLibraryOptionFailOnBinaryArchiveMiss;
constexpr auto MTLStitchedLibraryOptionNone = MTL::StitchedLibraryOptionNone;
constexpr auto MTLStitchedLibraryOptionStoreLibraryInMetalPipelinesScript = MTL::StitchedLibraryOptionStoreLibraryInMetalPipelinesScript;
constexpr auto MTLStorageModeManaged = MTL::StorageModeManaged;
constexpr auto MTLStorageModeMemoryless = MTL::StorageModeMemoryless;
constexpr auto MTLStorageModePrivate = MTL::StorageModePrivate;
constexpr auto MTLStorageModeShared = MTL::StorageModeShared;
constexpr auto MTLStoreActionCustomSampleDepthStore = MTL::StoreActionCustomSampleDepthStore;
constexpr auto MTLStoreActionDontCare = MTL::StoreActionDontCare;
constexpr auto MTLStoreActionMultisampleResolve = MTL::StoreActionMultisampleResolve;
constexpr auto MTLStoreActionOptionCustomSamplePositions = MTL::StoreActionOptionCustomSamplePositions;
constexpr auto MTLStoreActionOptionNone = MTL::StoreActionOptionNone;
constexpr auto MTLStoreActionOptionValidMask = MTL::StoreActionOptionValidMask;
constexpr auto MTLStoreActionStore = MTL::StoreActionStore;
constexpr auto MTLStoreActionStoreAndMultisampleResolve = MTL::StoreActionStoreAndMultisampleResolve;
constexpr auto MTLStoreActionUnknown = MTL::StoreActionUnknown;
constexpr auto MTLTensorDataTypeBFloat16 = MTL::TensorDataTypeBFloat16;
constexpr auto MTLTensorDataTypeFloat16 = MTL::TensorDataTypeFloat16;
constexpr auto MTLTensorDataTypeFloat32 = MTL::TensorDataTypeFloat32;
constexpr auto MTLTensorDataTypeInt16 = MTL::TensorDataTypeInt16;
constexpr auto MTLTensorDataTypeInt32 = MTL::TensorDataTypeInt32;
constexpr auto MTLTensorDataTypeInt4 = MTL::TensorDataTypeInt4;
constexpr auto MTLTensorDataTypeInt8 = MTL::TensorDataTypeInt8;
constexpr auto MTLTensorDataTypeNone = MTL::TensorDataTypeNone;
constexpr auto MTLTensorDataTypeUInt16 = MTL::TensorDataTypeUInt16;
constexpr auto MTLTensorDataTypeUInt32 = MTL::TensorDataTypeUInt32;
constexpr auto MTLTensorDataTypeUInt4 = MTL::TensorDataTypeUInt4;
constexpr auto MTLTensorDataTypeUInt8 = MTL::TensorDataTypeUInt8;
constexpr auto MTLTensorErrorInternalError = MTL::TensorErrorInternalError;
constexpr auto MTLTensorErrorInvalidDescriptor = MTL::TensorErrorInvalidDescriptor;
constexpr auto MTLTensorErrorNone = MTL::TensorErrorNone;
constexpr auto MTLTensorUsageCompute = MTL::TensorUsageCompute;
constexpr auto MTLTensorUsageMachineLearning = MTL::TensorUsageMachineLearning;
constexpr auto MTLTensorUsageRender = MTL::TensorUsageRender;
constexpr auto MTLTessellationControlPointIndexTypeNone = MTL::TessellationControlPointIndexTypeNone;
constexpr auto MTLTessellationControlPointIndexTypeUInt16 = MTL::TessellationControlPointIndexTypeUInt16;
constexpr auto MTLTessellationControlPointIndexTypeUInt32 = MTL::TessellationControlPointIndexTypeUInt32;
constexpr auto MTLTessellationFactorFormatHalf = MTL::TessellationFactorFormatHalf;
constexpr auto MTLTessellationFactorStepFunctionConstant = MTL::TessellationFactorStepFunctionConstant;
constexpr auto MTLTessellationFactorStepFunctionPerInstance = MTL::TessellationFactorStepFunctionPerInstance;
constexpr auto MTLTessellationFactorStepFunctionPerPatch = MTL::TessellationFactorStepFunctionPerPatch;
constexpr auto MTLTessellationFactorStepFunctionPerPatchAndPerInstance = MTL::TessellationFactorStepFunctionPerPatchAndPerInstance;
constexpr auto MTLTessellationPartitionModeFractionalEven = MTL::TessellationPartitionModeFractionalEven;
constexpr auto MTLTessellationPartitionModeFractionalOdd = MTL::TessellationPartitionModeFractionalOdd;
constexpr auto MTLTessellationPartitionModeInteger = MTL::TessellationPartitionModeInteger;
constexpr auto MTLTessellationPartitionModePow2 = MTL::TessellationPartitionModePow2;
constexpr auto MTLTextureCompressionTypeLossless = MTL::TextureCompressionTypeLossless;
constexpr auto MTLTextureCompressionTypeLossy = MTL::TextureCompressionTypeLossy;
constexpr auto MTLTextureSparseTier1 = MTL::TextureSparseTier1;
constexpr auto MTLTextureSparseTier2 = MTL::TextureSparseTier2;
constexpr auto MTLTextureSparseTierNone = MTL::TextureSparseTierNone;
constexpr auto MTLTextureSwizzleAlpha = MTL::TextureSwizzleAlpha;
constexpr auto MTLTextureSwizzleBlue = MTL::TextureSwizzleBlue;
constexpr auto MTLTextureSwizzleGreen = MTL::TextureSwizzleGreen;
constexpr auto MTLTextureSwizzleOne = MTL::TextureSwizzleOne;
constexpr auto MTLTextureSwizzleRed = MTL::TextureSwizzleRed;
constexpr auto MTLTextureSwizzleZero = MTL::TextureSwizzleZero;
constexpr auto MTLTextureType1D = MTL::TextureType1D;
constexpr auto MTLTextureType1DArray = MTL::TextureType1DArray;
constexpr auto MTLTextureType2D = MTL::TextureType2D;
constexpr auto MTLTextureType2DArray = MTL::TextureType2DArray;
constexpr auto MTLTextureType2DMultisample = MTL::TextureType2DMultisample;
constexpr auto MTLTextureType2DMultisampleArray = MTL::TextureType2DMultisampleArray;
constexpr auto MTLTextureType3D = MTL::TextureType3D;
constexpr auto MTLTextureTypeCube = MTL::TextureTypeCube;
constexpr auto MTLTextureTypeCubeArray = MTL::TextureTypeCubeArray;
constexpr auto MTLTextureTypeTextureBuffer = MTL::TextureTypeTextureBuffer;
constexpr auto MTLTextureUsagePixelFormatView = MTL::TextureUsagePixelFormatView;
constexpr auto MTLTextureUsageRenderTarget = MTL::TextureUsageRenderTarget;
constexpr auto MTLTextureUsageShaderAtomic = MTL::TextureUsageShaderAtomic;
constexpr auto MTLTextureUsageShaderRead = MTL::TextureUsageShaderRead;
constexpr auto MTLTextureUsageShaderWrite = MTL::TextureUsageShaderWrite;
constexpr auto MTLTextureUsageUnknown = MTL::TextureUsageUnknown;
constexpr auto MTLTransformTypeComponent = MTL::TransformTypeComponent;
constexpr auto MTLTransformTypePackedFloat4x3 = MTL::TransformTypePackedFloat4x3;
constexpr auto MTLTriangleFillModeFill = MTL::TriangleFillModeFill;
constexpr auto MTLTriangleFillModeLines = MTL::TriangleFillModeLines;
constexpr auto MTLVertexFormatChar = MTL::VertexFormatChar;
constexpr auto MTLVertexFormatChar2 = MTL::VertexFormatChar2;
constexpr auto MTLVertexFormatChar2Normalized = MTL::VertexFormatChar2Normalized;
constexpr auto MTLVertexFormatChar3 = MTL::VertexFormatChar3;
constexpr auto MTLVertexFormatChar3Normalized = MTL::VertexFormatChar3Normalized;
constexpr auto MTLVertexFormatChar4 = MTL::VertexFormatChar4;
constexpr auto MTLVertexFormatChar4Normalized = MTL::VertexFormatChar4Normalized;
constexpr auto MTLVertexFormatCharNormalized = MTL::VertexFormatCharNormalized;
constexpr auto MTLVertexFormatFloat = MTL::VertexFormatFloat;
constexpr auto MTLVertexFormatFloat2 = MTL::VertexFormatFloat2;
constexpr auto MTLVertexFormatFloat3 = MTL::VertexFormatFloat3;
constexpr auto MTLVertexFormatFloat4 = MTL::VertexFormatFloat4;
constexpr auto MTLVertexFormatFloatRG11B10 = MTL::VertexFormatFloatRG11B10;
constexpr auto MTLVertexFormatFloatRGB9E5 = MTL::VertexFormatFloatRGB9E5;
constexpr auto MTLVertexFormatHalf = MTL::VertexFormatHalf;
constexpr auto MTLVertexFormatHalf2 = MTL::VertexFormatHalf2;
constexpr auto MTLVertexFormatHalf3 = MTL::VertexFormatHalf3;
constexpr auto MTLVertexFormatHalf4 = MTL::VertexFormatHalf4;
constexpr auto MTLVertexFormatInt = MTL::VertexFormatInt;
constexpr auto MTLVertexFormatInt1010102Normalized = MTL::VertexFormatInt1010102Normalized;
constexpr auto MTLVertexFormatInt2 = MTL::VertexFormatInt2;
constexpr auto MTLVertexFormatInt3 = MTL::VertexFormatInt3;
constexpr auto MTLVertexFormatInt4 = MTL::VertexFormatInt4;
constexpr auto MTLVertexFormatInvalid = MTL::VertexFormatInvalid;
constexpr auto MTLVertexFormatShort = MTL::VertexFormatShort;
constexpr auto MTLVertexFormatShort2 = MTL::VertexFormatShort2;
constexpr auto MTLVertexFormatShort2Normalized = MTL::VertexFormatShort2Normalized;
constexpr auto MTLVertexFormatShort3 = MTL::VertexFormatShort3;
constexpr auto MTLVertexFormatShort3Normalized = MTL::VertexFormatShort3Normalized;
constexpr auto MTLVertexFormatShort4 = MTL::VertexFormatShort4;
constexpr auto MTLVertexFormatShort4Normalized = MTL::VertexFormatShort4Normalized;
constexpr auto MTLVertexFormatShortNormalized = MTL::VertexFormatShortNormalized;
constexpr auto MTLVertexFormatUChar = MTL::VertexFormatUChar;
constexpr auto MTLVertexFormatUChar2 = MTL::VertexFormatUChar2;
constexpr auto MTLVertexFormatUChar2Normalized = MTL::VertexFormatUChar2Normalized;
constexpr auto MTLVertexFormatUChar3 = MTL::VertexFormatUChar3;
constexpr auto MTLVertexFormatUChar3Normalized = MTL::VertexFormatUChar3Normalized;
constexpr auto MTLVertexFormatUChar4 = MTL::VertexFormatUChar4;
constexpr auto MTLVertexFormatUChar4Normalized = MTL::VertexFormatUChar4Normalized;
constexpr auto MTLVertexFormatUChar4Normalized_BGRA = MTL::VertexFormatUChar4Normalized_BGRA;
constexpr auto MTLVertexFormatUCharNormalized = MTL::VertexFormatUCharNormalized;
constexpr auto MTLVertexFormatUInt = MTL::VertexFormatUInt;
constexpr auto MTLVertexFormatUInt1010102Normalized = MTL::VertexFormatUInt1010102Normalized;
constexpr auto MTLVertexFormatUInt2 = MTL::VertexFormatUInt2;
constexpr auto MTLVertexFormatUInt3 = MTL::VertexFormatUInt3;
constexpr auto MTLVertexFormatUInt4 = MTL::VertexFormatUInt4;
constexpr auto MTLVertexFormatUShort = MTL::VertexFormatUShort;
constexpr auto MTLVertexFormatUShort2 = MTL::VertexFormatUShort2;
constexpr auto MTLVertexFormatUShort2Normalized = MTL::VertexFormatUShort2Normalized;
constexpr auto MTLVertexFormatUShort3 = MTL::VertexFormatUShort3;
constexpr auto MTLVertexFormatUShort3Normalized = MTL::VertexFormatUShort3Normalized;
constexpr auto MTLVertexFormatUShort4 = MTL::VertexFormatUShort4;
constexpr auto MTLVertexFormatUShort4Normalized = MTL::VertexFormatUShort4Normalized;
constexpr auto MTLVertexFormatUShortNormalized = MTL::VertexFormatUShortNormalized;
constexpr auto MTLVertexStepFunctionConstant = MTL::VertexStepFunctionConstant;
constexpr auto MTLVertexStepFunctionPerInstance = MTL::VertexStepFunctionPerInstance;
constexpr auto MTLVertexStepFunctionPerPatch = MTL::VertexStepFunctionPerPatch;
constexpr auto MTLVertexStepFunctionPerPatchControlPoint = MTL::VertexStepFunctionPerPatchControlPoint;
constexpr auto MTLVertexStepFunctionPerVertex = MTL::VertexStepFunctionPerVertex;
constexpr auto MTLVisibilityResultModeBoolean = MTL::VisibilityResultModeBoolean;
constexpr auto MTLVisibilityResultModeCounting = MTL::VisibilityResultModeCounting;
constexpr auto MTLVisibilityResultModeDisabled = MTL::VisibilityResultModeDisabled;
constexpr auto MTLVisibilityResultTypeAccumulate = MTL::VisibilityResultTypeAccumulate;
constexpr auto MTLVisibilityResultTypeReset = MTL::VisibilityResultTypeReset;
constexpr auto MTLWindingClockwise = MTL::WindingClockwise;
constexpr auto MTLWindingCounterClockwise = MTL::WindingCounterClockwise;

/* QuartzCore y Foundation. */
using NSString = NS::String;
using NSError = NS::Error;
using NSArray = NS::Array;
using NSAutoreleasePool = NS::AutoreleasePool;

#endif /* __OBJC__ */

/* Las cabeceras de este backend no solo declaran: tambien traen cuerpos de metodo
 * con envios de mensaje Objective-C (`[obj release]`). Para que el MISMO cuerpo
 * compile en los dos modos, el envio se encapsula aqui.
 *
 * Los dos caminos comprueban el nulo a proposito: en Objective-C mandar un mensaje a
 * `nil` es legal y no hace nada, pero en C++ puro `obj->release()` sobre nullptr
 * revienta. Sin esa guarda, el cambio de modo convertiria un no-op silencioso en un
 * fallo de segmentacion. */
template<typename T> inline void mtl_release(T obj)
{
  if (obj != nullptr) {
#ifdef __OBJC__
    [obj release];
#else
    obj->release();
#endif
  }
}

/* Crear una NSString desde texto C. Hace falta porque `MTLBuffer::set_label()` (en
 * mtl_memory.hh) recibe `NSString *`, y un `.cc` no puede escribir el literal `@"..."`.
 *
 * OJO CON LA PROPIEDAD: las dos ramas devuelven un objeto AUTOLIBERADO, igual que
 * hacia `[NSString stringWithFormat:]`. No se le manda `release`. En C++ puro eso
 * exige que haya un NS::AutoreleasePool vivo en el hilo; en el motor lo hay durante
 * el dibujado, pero si se llama fuera de uno, el objeto se filtra en vez de fallar
 * (un fallo silencioso, no un cierre). */
inline NSString *mtl_string(const char *text)
{
#ifdef __OBJC__
  return [NSString stringWithUTF8String:text];
#else
  return NS::String::string(text, NS::UTF8StringEncoding);
#endif
}
