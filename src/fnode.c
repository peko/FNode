// Includes
#include "fnode.h"      // Required for FNode framework functions

/// Global variables
Vector2 mousePosition = { 0, 0 };           // Current mouse position
Vector2 lastMousePosition = { 0, 0 };       // Previous frame mouse position
Vector2 mouseDelta = { 0, 0 };              // Current frame mouse position increment since previous frame
Vector2 currentOffset = { 0, 0 };           // Current selected node offset between mouse position and node shape
float modelRotation = 0.0f;                 // Current model visualization rotation angle
int scrollState = 0;                        // Current mouse drag interface scroll state
Vector2 canvasSize;                         // Interface screen size
float menuScroll = 10.0f;                   // Current interface scrolling amount
Vector2 scrollLimits = { 10, 1000 };        // Interface scrolling limits
Rectangle menuScrollRec = { 0, 0, 0, 0 };   // Interface scroll rectangle bounds
Vector2 menuScrollLimits = { 5, 685 };      // Interface scroll rectangle position limits
Rectangle canvasScroll = { 0, 0, 0, 0 };    // Interface scroll rectangle bounds
Model model;                                // Visor default model for shader visualization
RenderTexture2D visorTarget;                // Visor model visualization render target
Shader fxaa;                                // Canvas and visor anti-aliasing postprocessing shader
int fxaaUniform = -1;                       // FXAA shader viewport size uniform location point
Shader shader;                              // Visor model shader
int viewUniform = -1;                       // Created shader view direction uniform location point
int transformUniform = -1;                  // Created shader model transform uniform location point
bool loadedShader = false;                  // Current loaded custom shader state

// Functions declarations
void CheckPreviousShader();                                 // Check if there are a compatible shader in output folder
void UpdateMouseData();                                     // Updates current mouse position and delta position
void UpdateCanvas();                                        // Updates canvas space target and offset
void UpdateScroll();                                        // Updates mouse scrolling for menu and canvas drag
void UpdateNodesEdit();                                     // Check node data values edit input
void UpdateNodesDrag();                                     // Check node drag input
void UpdateNodesLink();                                     // Check node link input
void UpdateCommentCreationEdit();                           // Check comment creation and shape edit input
void UpdateCommentsDrag();                                  // Check comment drag input
void UpdateCommentsEdit();                                  // Check comment text edit input
void UpdateShaderData();                                    // Update required values to created shader for geometry data calculations
void CompileShader();                                       // Compiles all node structure to create the GLSL fragment shader in output folder
void CheckConstant(FNode node, FILE *file);                 // Check nodes searching for constant values to define them in shaders
void CompileNode(FNode node, FILE *file, bool fragment);    // Compiles a specific node checking its inputs and writing current node operation in shader
void AlignAllNodes();                                       // Aligns all created nodes
void ClearUnusedNodes();                                    // Destroys all unused nodes
void ClearGraph();                                          // Destroys all created nodes and its linked lines
void DrawCanvas();                                          // Draw canvas space to create nodes
void DrawCanvasGrid(int divisions);                         // Draw canvas grid with a specific number of divisions for horizontal and vertical lines
void DrawVisor();                                           // Draws a visor with default model rotating and current shader
void DrawInterface();                                       // Draw interface to create nodes

int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(screenSize.x, screenSize.y, "fnode 1.0");
    SetTargetFPS(60);
    SetLineWidth(3);

    // Load resources
    model = LoadModel("res/model.obj");
    visorTarget = LoadRenderTexture(screenSize.x/4, screenSize.y/4);
    fxaa = LoadShader("res/fxaa.vs", "res/fxaa.fs");

    // Initialize values
    camera = (Camera2D){ (Vector2){ 0, 0 }, (Vector2){ screenSize.x/2, screenSize.y/2 }, 0.0f, 1.0f };
    canvasSize = (Vector2){ screenSize.x*0.85f, screenSize.y };
    camera3d = (Camera){{ 0.0f, 0.0f, 4.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f };
    menuScrollRec = (Rectangle){ screenSize.x - 17, 5, 9, 30 };

    // Initialize shaders values
    fxaaUniform = GetShaderLocation(fxaa, "viewportSize");
    float viewportSize[2] = { screenSize.x/4, screenSize.y/4 };
    SetShaderValue(fxaa, fxaaUniform, viewportSize, 2);

    // Setup orbital camera
    SetCameraPosition(camera3d.position);     // Set internal camera position to match our camera position
    SetCameraTarget(camera3d.target);         // Set internal camera target to match our camera target

    InitFNode();
    CheckPreviousShader();
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateMouseData();
        UpdateCanvas();
        UpdateScroll();
        UpdateNodesEdit();
        UpdateNodesDrag();
        UpdateNodesLink();
        UpdateCommentCreationEdit();
        UpdateCommentsEdit();
        UpdateCommentsDrag();
        UpdateShaderData();

        if (IsKeyPressed('P')) debugMode = !debugMode;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);
            DrawCanvas();
            DrawInterface();
            DrawVisor();

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(visorTarget);
    UnloadModel(model);
    UnloadShader(fxaa);
    if (loadedShader) UnloadShader(shader);

    CloseFNode();
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

// Check if there are a compatible shader in output folder
void CheckPreviousShader()
{
    Shader previousShader = LoadShader(VERTEX_PATH, FRAGMENT_PATH);
    if (previousShader.id != 0)
    {
        shader = previousShader;
        model.material.shader = shader;
        viewUniform = GetShaderLocation(shader, "viewDirection");
        transformUniform = GetShaderLocation(shader, "modelMatrix");

        FILE *dataFile = fopen(DATA_PATH, "r");
        if (dataFile != NULL)
        {
            float type = -1;
            float inputs[MAX_INPUTS] = { -1, -1, -1, -1 };
            float inputsCount = -1;
            float inputsLimit = -1;
            float dataCount = -1;
            float data[MAX_VALUES] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            float shapeX = -1;
            float shapeY = -1;

            while (fscanf(dataFile, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,\n", &type,
            &inputs[0], &inputs[1], &inputs[2], &inputs[3], &inputsCount, &inputsLimit, &dataCount, &data[0], &data[1], &data[2],
            &data[3], &data[4], &data[5], &data[6], &data[7], &data[8], &data[9], &data[10], &data[11], &data[12], &data[13], &data[14],
            &data[15], &shapeX, &shapeY) > 0)
            {                
                FNode newNode = InitializeNode(true);
                newNode->type = type;

                if (type < FNODE_ADD) newNode->inputShape = (Rectangle){ 0, 0, 0, 0 };

                switch ((int)type)
                {
                    case FNODE_PI: newNode->name = "Pi"; break;
                    case FNODE_E: newNode->name = "e"; break;
                    case FNODE_VERTEXPOSITION: newNode->name = "Vertex Position"; break;
                    case FNODE_VERTEXNORMAL: newNode->name = "Normal Direction"; break;
                    case FNODE_FRESNEL: newNode->name = "Fresnel"; break;
                    case FNODE_VIEWDIRECTION: newNode->name = "View Direction"; break;
                    case FNODE_MVP: newNode->name = "MVP Matrix"; break;
                    case FNODE_MATRIX: newNode->name = "Matrix 4x4"; break;
                    case FNODE_VALUE: newNode->name = "Value"; break;
                    case FNODE_VECTOR2: newNode->name = "Vector 2"; break;
                    case FNODE_VECTOR3: newNode->name = "Vector 3"; break;
                    case FNODE_VECTOR4: newNode->name = "Vector 4"; break;
                    case FNODE_ADD: newNode->name = "Add"; break;
                    case FNODE_SUBTRACT: newNode->name = "Subtract"; break;
                    case FNODE_MULTIPLY: newNode->name = "Multiply"; break;
                    case FNODE_DIVIDE: newNode->name = "Divide"; break;
                    case FNODE_APPEND: newNode->name = "Append"; break;
                    case FNODE_ONEMINUS: newNode->name = "One Minus"; break;
                    case FNODE_ABS: newNode->name = "Abs"; break;
                    case FNODE_COS:newNode->name = "Cos"; break;
                    case FNODE_SIN: newNode->name = "Sin"; break;
                    case FNODE_TAN: newNode->name = "Tan"; break;
                    case FNODE_DEG2RAD: newNode->name = "Deg to Rad"; break;
                    case FNODE_RAD2DEG: newNode->name = "Rad to Deg"; break;
                    case FNODE_NORMALIZE: newNode->name = "Normalize"; break;
                    case FNODE_NEGATE: newNode->name = "Negate"; break;
                    case FNODE_RECIPROCAL: newNode->name = "Reciprocal"; break;
                    case FNODE_SQRT: newNode->name = "Square Root"; break;
                    case FNODE_TRUNC: newNode->name = "Trunc"; break;
                    case FNODE_ROUND: newNode->name = "Round"; break;
                    case FNODE_CEIL: newNode->name = "Ceil"; break;
                    case FNODE_CLAMP01: newNode->name = "Clamp 0-1"; break;
                    case FNODE_EXP2: newNode->name = "Exp 2"; break;
                    case FNODE_POWER: newNode->name = "Power"; break;
                    case FNODE_STEP: newNode->name = "Step"; break;
                    case FNODE_POSTERIZE: newNode->name = "Posterize"; break;
                    case FNODE_MAX: newNode->name = "Max"; break;
                    case FNODE_MIN: newNode->name = "Min"; break;
                    case FNODE_LERP: newNode->name = "Lerp"; break;
                    case FNODE_SMOOTHSTEP: newNode->name = "Smooth Step"; break;
                    case FNODE_CROSSPRODUCT: newNode->name = "Cross Product"; break;
                    case FNODE_DESATURATE: newNode->name = "Desaturate"; break;
                    case FNODE_DISTANCE: newNode->name = "Distance"; break;
                    case FNODE_DOTPRODUCT: newNode->name = "Dot Product"; break;
                    case FNODE_LENGTH: newNode->name = "Length"; break;
                    case FNODE_MULTIPLYMATRIX: newNode->name = "Multiply Matrix"; break;
                    case FNODE_TRANSPOSE: newNode->name = "Transpose"; break;
                    case FNODE_PROJECTION: newNode->name = "Projection Vector"; break;
                    case FNODE_REJECTION: newNode->name = "Rejection Vector"; break;
                    case FNODE_HALFDIRECTION: newNode->name = "Half Direction"; break;
                    case FNODE_VERTEX: newNode->name = "Final Vertex Position"; break;
                    case FNODE_FRAGMENT: newNode->name = "Final Fragment Color"; break;
                    default: break;
                }

                for (int i = 0; i < MAX_INPUTS; i++) newNode->inputs[i] = inputs[i];

                newNode->inputsCount = inputsCount;
                newNode->inputsLimit = inputsLimit;

                for (int i = 0; i < MAX_VALUES; i++)
                {
                    newNode->output.data[i].value = data[i];
                    FFloatToString(newNode->output.data[i].valueText, newNode->output.data[i].value);
                }

                newNode->output.dataCount = dataCount;
                newNode->shape.x = shapeX;
                newNode->shape.y = shapeY;

                UpdateNodeShapes(newNode);
            }

            int from = -1;
            int to = -1;            
            while (fscanf(dataFile, "?%i?%i\n", &from, &to) > 0)
            {
                tempLine = CreateNodeLine(from);
                tempLine->to = to;
            }

            for (int i = 0; i < nodesCount; i++) UpdateNodeShapes(nodes[i]);
            CalculateValues();
            for (int i = 0; i < nodesCount; i++) UpdateNodeShapes(nodes[i]);

            loadedShader = true;
            fclose(dataFile);
        }
        else TraceLogFNode(false, "error when trying to open previous shader data file");
    }
    
    if (!loadedShader)
    {
        CreateNodeMaterial(FNODE_VERTEX, "Final Vertex Position", 0);
        CreateNodeMaterial(FNODE_FRAGMENT, "Final Fragment Color", 0);
    }
}

// Updates current mouse position and delta position
void UpdateMouseData()
{
    // Update mouse position values
    lastMousePosition = mousePosition;
    mousePosition = GetMousePosition();
    mouseDelta = (Vector2){ mousePosition.x - lastMousePosition.x, mousePosition.y - lastMousePosition.y };
}

// Updates canvas space target and offset
void UpdateCanvas()
{
    // Update canvas camera values
    camera.target = mousePosition;

    // Update visor model current rotation
    modelRotation -= VISOR_MODEL_ROTATION;
}

// Updates mouse scrolling for menu and canvas drag
void UpdateScroll()
{
    // Check zoom input
    if (GetMouseWheelMove() != 0)
    {
        if (CheckCollisionPointRec(mousePosition, (Rectangle){ canvasSize.x - visorTarget.texture.width - UI_PADDING, screenSize.y - visorTarget.texture.height - UI_PADDING, visorTarget.texture.width, visorTarget.texture.height }))
        {
            camera3d.position.z += GetMouseWheelMove()*0.25f;
            camera3d.position.z = FClamp(camera3d.position.z, 2.5f, 6.0f);
        }
        else if (CheckCollisionPointRec(mousePosition, (Rectangle){ 0, 0, canvasSize.x, canvasSize.y }))
        {
            if (IsKeyDown(KEY_LEFT_ALT)) camera.offset.x -= GetMouseWheelMove()*UI_SCROLL;
            else camera.offset.y -= GetMouseWheelMove()*UI_SCROLL;
        }
        else
        {
            menuScroll -= GetMouseWheelMove()*UI_SCROLL;
            menuScroll = FClamp(menuScroll, scrollLimits.x, scrollLimits.y);
            menuScrollRec.y = (menuScrollLimits.y - menuScrollLimits.x)*menuScroll/(scrollLimits.y - scrollLimits.x);
        }
    }

    // Check mouse drag interface scrolling input
    if (scrollState == 0)
    {
        if ((IsMouseButtonDown(MOUSE_LEFT_BUTTON)) && (CheckCollisionPointRec(mousePosition, menuScrollRec))) scrollState = 1;
    }
    else
    {
        menuScroll += mouseDelta.y*1.45f;
        menuScrollRec.y += mouseDelta.y;

        if (menuScrollRec.y >= menuScrollLimits.y)
        {
            menuScroll = scrollLimits.y;
            menuScrollRec.y = menuScrollLimits.y;
        }
        else if (menuScrollRec.y <= menuScrollLimits.x)
        {
            menuScroll = scrollLimits.x;
            menuScrollRec.y = menuScrollLimits.x;
        }

        if (IsMouseButtonUp(MOUSE_LEFT_BUTTON)) scrollState = 0;
    }
}

// Check node data values edit input
void UpdateNodesEdit()
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        int index = -1;
        int data = -1;
        for (int i = 0; i < nodesCount; i++)
        {
            if ((nodes[i]->type >= FNODE_MATRIX) && (nodes[i]->type <= FNODE_VECTOR4))
            {
                for (int k = 0; k < nodes[i]->output.dataCount; k++)
                {
                    if (CheckCollisionPointRec(mousePosition, CameraToViewRec(nodes[i]->output.data[k].shape, camera)))
                    {
                        index = i;
                        data = k;
                        break;
                    }
                }
            }
        }

        if (index != -1)
        {
            if ((editNode == -1) && (selectedNode == -1) && (lineState == 0) && (commentState == 0) && (selectedComment == -1) && (editSize == -1) && (editSizeType == -1) && (editComment == -1))
            {
                editNode = nodes[index]->id;
                editNodeType = data;
                editNodeText = (char *)FNODE_MALLOC(MAX_NODE_LENGTH);
                usedMemory += MAX_NODE_LENGTH;
                for (int i = 0; i < MAX_NODE_LENGTH; i++) editNodeText[i] = nodes[index]->output.data[data].valueText[i];
            }
            else if ((editNode != -1) && (selectedNode == -1) && (lineState == 0) && (commentState == 0) && (selectedComment == -1) && (editSize == -1) && (editSizeType == -1) && (editComment == -1))
            {
                if ((nodes[index]->id != editNode) || (data != editNodeType))
                {
                    for (int i = 0; i < nodesCount; i++)
                    {
                        if (nodes[i]->id == editNode)
                        {
                            for (int k = 0; k < MAX_NODE_LENGTH; k++) nodes[i]->output.data[editNodeType].valueText[k] = editNodeText[k];
                        }
                    }

                    editNode = nodes[index]->id;
                    editNodeType = data;

                    for (int i = 0; i < MAX_NODE_LENGTH; i++) editNodeText[i] = nodes[index]->output.data[data].valueText[i];
                }
            }
        }
        else if ((editNode != -1) && (editNodeType != -1))
        {
            for (int i = 0; i < nodesCount; i++)
            {
                if (nodes[i]->id == editNode)
                {
                    for (int k = 0; k < MAX_NODE_LENGTH; k++) nodes[i]->output.data[editNodeType].valueText[k] = editNodeText[k];
                }
            }

            editNode = -1;
            editNodeType = -1;
            FNODE_FREE(editNodeText);
            usedMemory -= MAX_NODE_LENGTH;
            editNodeText = NULL;
        }     
    }
}

// Check node drag input
void UpdateNodesDrag()
{
    if ((selectedNode == -1) && (lineState == 0) && (commentState == 0) && (selectedComment == -1))
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            for (int i = nodesCount - 1; i >= 0; i--)
            {
                if (CheckCollisionPointRec(mousePosition, CameraToViewRec(nodes[i]->shape, camera)))
                {
                    selectedNode = nodes[i]->id;
                    currentOffset = (Vector2){ mousePosition.x - nodes[i]->shape.x, mousePosition.y - nodes[i]->shape.y };
                    break;
                }
            }

            if ((selectedNode == -1) && (scrollState == 0) && (!CheckCollisionPointRec(mousePosition, (Rectangle){ canvasSize.x, 0, (screenSize.x - canvasSize.x), screenSize.y })))
            {
                camera.offset.x += mouseDelta.x;
                camera.offset.y += mouseDelta.y;
            }
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
        {
            for (int i = nodesCount - 1; i >= 0; i--)
            {
                if (CheckCollisionPointRec(mousePosition, CameraToViewRec(nodes[i]->shape, camera)) && (nodes[i]->type < FNODE_VERTEX))
                {
                    DestroyNode(nodes[i]);
                    CalculateValues();
                    break;
                }
            }
        }
    }
    else if ((selectedNode != -1) && (lineState == 0) && (commentState == 0) && (selectedComment == -1))
    {
        for (int i = 0; i < nodesCount; i++)
        {
            if (nodes[i]->id == selectedNode)
            {
                nodes[i]->shape.x = mousePosition.x - currentOffset.x;
                nodes[i]->shape.y = mousePosition.y - currentOffset.y;

                // Check aligned drag movement input
                if (IsKeyDown(KEY_LEFT_ALT)) AlignNode(nodes[i]);

                UpdateNodeShapes(nodes[i]);
                break;
            }
        }

        if (IsMouseButtonUp(MOUSE_LEFT_BUTTON)) selectedNode = -1;
    }
}

// Check node link input
void UpdateNodesLink()
{
    if ((selectedNode == -1) && (commentState == 0) && (selectedComment == -1)) 
    {
        switch (lineState)
        {
            case 0:
            {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    for (int i = nodesCount - 1; i >= 0; i--)
                    {
                        if (CheckCollisionPointRec(mousePosition, CameraToViewRec(nodes[i]->outputShape, camera)))
                        {
                            tempLine = CreateNodeLine(nodes[i]->id);
                            lineState = 1;
                            break;
                        }
                    }
                }
                else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !IsKeyDown(KEY_LEFT_ALT))
                {
                    for (int i = nodesCount - 1; i >= 0; i--)
                    {
                        if (CheckCollisionPointRec(mousePosition, CameraToViewRec(nodes[i]->outputShape, camera)))
                        {
                            for (int k = linesCount - 1; k >= 0; k--)
                            {
                                if (nodes[i]->id == lines[k]->from) DestroyNodeLine(lines[k]);
                            }

                            CalculateValues();
                            CalculateValues();
                            break;
                        }
                        else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(nodes[i]->inputShape, camera)))
                        {
                            for (int k = linesCount - 1; k >= 0; k--)
                            {
                                if (nodes[i]->id == lines[k]->to) DestroyNodeLine(lines[k]);
                            }

                            CalculateValues();
                            CalculateValues();
                            break;
                        }
                    }
                }
            } break;
            case 1:
            {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    for (int i = 0; i < nodesCount; i++)
                    {
                        if (CheckCollisionPointRec(mousePosition, CameraToViewRec(nodes[i]->inputShape, camera)) && (nodes[i]->id != tempLine->from) && (nodes[i]->inputsCount < nodes[i]->inputsLimit))
                        {
                            // Get which index has the first input node id from current nude                            
                            int indexFrom = GetNodeIndex(tempLine->from);

                            bool valuesCheck = true;
                            if (nodes[i]->type == FNODE_APPEND) valuesCheck = ((nodes[i]->output.dataCount + nodes[indexFrom]->output.dataCount <= 4) && (nodes[indexFrom]->output.dataCount == 1));
                            else if ((nodes[i]->type == FNODE_POWER) && (nodes[i]->inputsCount == 1)) valuesCheck = (nodes[indexFrom]->output.dataCount == 1);
                            else if (nodes[i]->type == FNODE_STEP) valuesCheck = (nodes[indexFrom]->output.dataCount == 1);
                            else if (nodes[i]->type == FNODE_NORMALIZE) valuesCheck = ((nodes[indexFrom]->output.dataCount > 1) && (nodes[indexFrom]->output.dataCount <= 4));
                            else if (nodes[i]->type == FNODE_CROSSPRODUCT) valuesCheck = (nodes[indexFrom]->output.dataCount == 3);
                            else if (nodes[i]->type == FNODE_DESATURATE)
                            {
                                if (nodes[i]->inputsCount == 0) valuesCheck = (nodes[indexFrom]->output.dataCount < 4);
                                else if (nodes[i]->inputsCount == 1) valuesCheck = (nodes[indexFrom]->output.dataCount == 1);
                            }
                            else if ((nodes[i]->type == FNODE_DOTPRODUCT) || (nodes[i]->type == FNODE_LENGTH) || ((nodes[i]->type >= FNODE_PROJECTION) && (nodes[i]->type <= FNODE_HALFDIRECTION)))
                            {
                                valuesCheck = ((nodes[indexFrom]->output.dataCount > 1) && (nodes[indexFrom]->output.dataCount <= 4));

                                if (valuesCheck && (nodes[i]->inputsCount > 0))
                                {
                                    int index = GetNodeIndex(nodes[i]->inputs[0]);
                                    
                                    if (index != -1) valuesCheck = (nodes[indexFrom]->output.dataCount == nodes[index]->output.dataCount);
                                    else TraceLogFNode(true, "error when trying to get node inputs index");
                                }
                            }
                            else if (nodes[i]->type == FNODE_DISTANCE)
                            {
                                valuesCheck = ((nodes[indexFrom]->output.dataCount <= 4));

                                if (valuesCheck && (nodes[i]->inputsCount > 0))
                                {
                                    int index = GetNodeIndex(nodes[i]->inputs[0]);
                                    
                                    if (index != -1) valuesCheck = (nodes[indexFrom]->output.dataCount == nodes[index]->output.dataCount);
                                    else TraceLogFNode(true, "error when trying to get node inputs index");
                                }
                            }
                            else if ((nodes[i]->type == FNODE_MULTIPLYMATRIX) || (nodes[i]->type == FNODE_TRANSPOSE)) valuesCheck = (nodes[indexFrom]->output.dataCount == 16);
                            else if (nodes[i]->type >= FNODE_VERTEX) valuesCheck = (nodes[indexFrom]->output.dataCount <= nodes[i]->output.dataCount);
                            else if (nodes[i]->type > FNODE_DIVIDE) valuesCheck = (nodes[i]->output.dataCount == nodes[indexFrom]->output.dataCount);

                            if (((nodes[i]->inputsCount == 0) && (nodes[i]->type != FNODE_NORMALIZE) && (nodes[i]->type != FNODE_DOTPRODUCT) && 
                            (nodes[i]->type != FNODE_LENGTH) && (nodes[i]->type != FNODE_MULTIPLYMATRIX) && (nodes[i]->type != FNODE_TRANSPOSE) && (nodes[i]->type != FNODE_PROJECTION) &&
                            (nodes[i]->type != FNODE_DISTANCE) && (nodes[i]->type != FNODE_REJECTION) && (nodes[i]->type != FNODE_HALFDIRECTION) && (nodes[i]->type != FNODE_STEP)) || valuesCheck)
                            {
                                // Check if there is already a line created with same linking ids
                                for (int k = 0; k < linesCount; k++)
                                {
                                    if ((lines[k]->to == nodes[i]->id) && (lines[k]->from == tempLine->from))
                                    {
                                        DestroyNodeLine(lines[k]);
                                        break;
                                    }
                                }

                                // Save temporal line values and destroy it
                                int from = tempLine->from;
                                int to = nodes[i]->id;
                                DestroyNodeLine(tempLine);

                                // Create final node line
                                FLine temp = CreateNodeLine(from);
                                temp->to = to;

                                // Reset linking state values
                                lineState = 0;
                                CalculateValues();
                                CalculateValues();
                                break;
                            }
                            else TraceLogFNode(false, "error trying to link node ID %i (length: %i) with node ID %i (length: %i)", nodes[i]->id, nodes[i]->output.dataCount, nodes[indexFrom]->id, nodes[indexFrom]->output.dataCount);
                        }
                    }
                }
                else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
                {
                    DestroyNodeLine(tempLine);
                    lineState = 0;
                }
            } break;
            default: break;
        }
    }
}

// Check comment creation input
void UpdateCommentCreationEdit()
{
    if ((selectedNode == -1) && (lineState == 0) && (selectedComment == -1)) 
    {
        switch (commentState)
        {
            case 0:
            {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    if (IsKeyDown(KEY_LEFT_ALT))
                    {
                        commentState = 1;

                        tempCommentPos.x = mousePosition.x;
                        tempCommentPos.y = mousePosition.y;

                        tempComment = CreateComment();
                        tempComment->shape.x = mousePosition.x - camera.offset.x;
                        tempComment->shape.y = mousePosition.y - camera.offset.y;

                        UpdateCommentShapes(tempComment);
                    }
                    else
                    {
                        for (int i = 0; i < commentsCount; i++)
                        {
                            if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeTShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 0;
                                break;
                            }
                            else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeBShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 1;
                                break;
                            }
                            else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeLShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 2;
                                break;
                            }
                            else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeRShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 3;
                                break;
                            }
                            else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeTlShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 4;
                                break;
                            }
                            else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeTrShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 5;
                                break;
                            }
                            else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeBlShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 6;
                                break;
                            }
                            else if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->sizeBrShape, camera)))
                            {
                                editSize = comments[i]->id;
                                commentState = 1;
                                editSizeType = 7;
                                break;
                            }
                        }
                    }
                }
            } break;
            case 1:
            {
                if (editSize != -1)
                {
                    for (int i = 0; i < commentsCount; i++)
                    {
                        if (comments[i]->id == editSize)
                        {
                            switch (editSizeType)
                            {
                                case 0:
                                {
                                    comments[i]->shape.y += mouseDelta.y;
                                    comments[i]->shape.height -= mouseDelta.y;
                                } break;
                                case 1: comments[i]->shape.height += mouseDelta.y; break;
                                case 2:
                                {
                                    comments[i]->shape.x += mouseDelta.x;
                                    comments[i]->shape.width -= mouseDelta.x;
                                } break;
                                case 3: comments[i]->shape.width += mouseDelta.x; break;
                                case 4:
                                {
                                    comments[i]->shape.x += mouseDelta.x;
                                    comments[i]->shape.width -= mouseDelta.x;
                                    comments[i]->shape.y += mouseDelta.y;
                                    comments[i]->shape.height -= mouseDelta.y;
                                } break;
                                case 5:
                                {
                                    comments[i]->shape.width += mouseDelta.x;
                                    comments[i]->shape.y += mouseDelta.y;
                                    comments[i]->shape.height -= mouseDelta.y;
                                } break;
                                case 6:
                                {
                                    comments[i]->shape.x += mouseDelta.x;
                                    comments[i]->shape.width -= mouseDelta.x;
                                    comments[i]->shape.height += mouseDelta.y;
                                } break;
                                case 7:
                                {
                                    comments[i]->shape.width += mouseDelta.x;
                                    comments[i]->shape.height += mouseDelta.y;
                                } break;
                                default: break;
                            }

                            UpdateCommentShapes(comments[i]);
                            break;
                        }
                    }

                    if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
                    {
                        editSize = -1;
                        editSizeType = -1;
                        commentState = 0;
                    }
                }
                else
                {
                    if ((mousePosition.x - tempCommentPos.x) >= 0) tempComment->shape.width = mousePosition.x - tempComment->shape.x - camera.offset.x;
                    else
                    {
                        tempComment->shape.width = tempCommentPos.x - mousePosition.x;
                        tempComment->shape.x = tempCommentPos.x - tempComment->shape.width - camera.offset.x;
                    }

                    if ((mousePosition.y - tempCommentPos.y) >= 0) tempComment->shape.height = mousePosition.y - tempComment->shape.y - camera.offset.y;
                    else
                    {
                        tempComment->shape.height = tempCommentPos.y - mousePosition.y;
                        tempComment->shape.y = tempCommentPos.y - tempComment->shape.height - camera.offset.y;
                    }

                    UpdateCommentShapes(tempComment);

                    if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
                    {
                        // Save temporal comment values
                        Rectangle tempRec = { tempComment->shape.x, tempComment->shape.y, tempComment->shape.width, tempComment->shape.height };
                        DestroyComment(tempComment);

                        // Reset comment state
                        commentState = 0;

                        if (tempRec.width >= 0 && tempRec.height >= 0)
                        {
                            // Create final comment
                            FComment temp = CreateComment();
                            temp->shape = tempRec;
                            
                            UpdateCommentShapes(temp);
                        }
                        else TraceLogFNode(false, "comment have not been created because its width or height are has a negative value");
                    }
                }
            } break;
            default: break;
        }
    }
}

// Check comment drag input
void UpdateCommentsDrag()
{
    if ((selectedComment == -1) && (lineState == 0) && (commentState == 0) && (selectedNode == -1))
    {
        if (!IsKeyDown(KEY_LEFT_ALT))
        {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            {
                for (int i = commentsCount - 1; i >= 0; i--)
                {
                    if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->shape, camera)))
                    {
                        selectedComment = comments[i]->id;
                        currentOffset = (Vector2){ mousePosition.x - comments[i]->shape.x, mousePosition.y - comments[i]->shape.y };

                        for (int k = 0; k < nodesCount; k++)
                        {
                            if (CheckCollisionRecs(CameraToViewRec(comments[i]->shape, camera), CameraToViewRec(nodes[k]->shape, camera)))
                            {
                                selectedCommentNodes[selectedCommentNodesCount] = nodes[k]->id;
                                selectedCommentNodesCount++;

                                if (selectedCommentNodesCount > MAX_NODES) break;
                            }
                        }

                        break;
                    }
                }
            }
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
        {
            for (int i = commentsCount - 1; i >= 0; i--)
            {
                if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->shape, camera)))
                {
                    DestroyComment(comments[i]);
                    break;
                }
            }
        }
    }
    else if ((selectedComment != -1) && (lineState == 0) && (commentState == 0) && (selectedNode == -1))
    {
        for (int i = 0; i < commentsCount; i++)
        {
            if (comments[i]->id == selectedComment)
            {
                comments[i]->shape.x = mousePosition.x - currentOffset.x;
                comments[i]->shape.y = mousePosition.y - currentOffset.y;

                UpdateCommentShapes(comments[i]);

                for (int k = 0; k < selectedCommentNodesCount; k++)
                {
                    for (int j = 0; j < nodesCount; j++)
                    {
                        if (nodes[j]->id == selectedCommentNodes[k])
                        {
                            nodes[j]->shape.x += mouseDelta.x;
                            nodes[j]->shape.y += mouseDelta.y;

                            UpdateNodeShapes(nodes[j]);
                            break;
                        }
                    }
                }
                break;
            }
        }

        if (IsMouseButtonUp(MOUSE_LEFT_BUTTON))
        {
            selectedComment = -1;

            for (int i = 0; i < selectedCommentNodesCount; i++) selectedCommentNodes[i] = -1;
            selectedCommentNodesCount = 0;
        }
    }
}

// Check comment text edit input
void UpdateCommentsEdit()
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if ((editComment == -1) && (selectedNode == -1) && (lineState == 0) && (commentState == 0) && (selectedComment == -1) && (editSize == -1) && (editSizeType == -1) && (editNode == -1))
        {
            for (int i = 0; i < commentsCount; i++)
            {
                if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->valueShape, camera)))
                {
                    editComment = i;
                    break;
                }
            }
        }
        else if ((editComment != -1) && (selectedNode == -1) && (lineState == 0) && (commentState == 0) && (selectedComment == -1) && (editSize == -1) && (editSizeType == -1) && (editNode == -1))
        {
            bool isCurrentText = false;
            int currentEdit = editComment;
            for (int i = 0; i < commentsCount; i++)
            {
                if (comments[i]->id == editComment)
                {
                    if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->valueShape, camera)))
                    {
                        isCurrentText = true;
                        break;
                    }
                }

                if (CheckCollisionPointRec(mousePosition, CameraToViewRec(comments[i]->valueShape, camera)))
                {
                    editComment = i;
                    break;
                }
            }

            // Reset current editing text any other text label is pressed
            if (!isCurrentText && (currentEdit == editComment)) editComment = -1;
        }
    }
}

// Update required values to created shader for geometry data calculations
void UpdateShaderData()
{
    if (shader.id != 0)
    {
        Vector3 viewVector = { camera3d.position.x - camera3d.target.x, camera3d.position.y - camera3d.target.y, camera3d.position.z - camera3d.target.z };
        viewVector = FVector3Normalize(viewVector);
        float viewDir[3] = {  viewVector.x, viewVector.y, viewVector.z };
        SetShaderValue(shader, viewUniform, viewDir, 3);

        SetShaderValueMatrix(shader, transformUniform, model.transform);
    }
}

// Compiles all node structure to create the GLSL fragment shader in output folder
void CompileShader()
{
    if (loadedShader) UnloadShader(shader);
    remove(DATA_PATH);
    remove(VERTEX_PATH);
    remove(FRAGMENT_PATH);

    // Open shader data file
    FILE *dataFile = fopen(DATA_PATH, "w");
    if (dataFile != NULL)
    {
        // Nodes data reading
        int count = 0;
        for (int i = 0; i < MAX_NODES; i++)
        {
            for (int k = 0; k < nodesCount; k++)
            {
                if (nodes[k]->id == i)
                {
                    float type = (float)nodes[k]->type;
                    float inputs[MAX_INPUTS] = { (float)nodes[k]->inputs[0], (float)nodes[k]->inputs[1], (float)nodes[k]->inputs[2], (float)nodes[k]->inputs[3] };
                    float inputsCount = (float)nodes[k]->inputsCount;
                    float inputsLimit = (float)nodes[k]->inputsLimit;
                    float dataCount = (float)nodes[k]->output.dataCount;
                    float data[MAX_VALUES] = { nodes[k]->output.data[0].value, nodes[k]->output.data[1].value, nodes[k]->output.data[2].value, nodes[k]->output.data[3].value, nodes[k]->output.data[4].value,
                    nodes[k]->output.data[5].value, nodes[k]->output.data[6].value, nodes[k]->output.data[7].value, nodes[k]->output.data[8].value, nodes[k]->output.data[9].value, nodes[k]->output.data[10].value,
                    nodes[k]->output.data[11].value, nodes[k]->output.data[12].value, nodes[k]->output.data[13].value, nodes[k]->output.data[14].value, nodes[k]->output.data[15].value };
                    float shapeX = (float)nodes[k]->shape.x;
                    float shapeY = (float)nodes[k]->shape.y;

                    fprintf(dataFile, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,\n", type,
                    inputs[0], inputs[1], inputs[2], inputs[3], inputsCount, inputsLimit, dataCount, data[0], data[1], data[2],
                    data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[11], data[12], data[13], data[14],
                    data[15], shapeX, shapeY);
                    
                    count++;
                    break;
                }
            }

            if (count == nodesCount) break;
        }

        // Lines data reading
        count = 0;

        for (int i = 0; i < MAX_NODES; i++)
        {
            for (int k = 0; k < nodesCount; k++)
            {
                if (lines[k]->id == i)
                {
                    fprintf(dataFile, "?%i?%i\n", lines[k]->from, lines[k]->to);
                    
                    count++;
                    break;
                }
            }

            if (count == linesCount) break;
        }

        fclose(dataFile);
    }
    else TraceLogFNode(true, "error when trying to open and write in data file");

    // Open vertex shader file to write data
    FILE *vertexFile = fopen(VERTEX_PATH, "w");
    if (vertexFile != NULL)
    {
        // Vertex shader definition to embed, no external file required
        const char vHeader[] = 
        "#version 330                     \n\n";
        fprintf(vertexFile, vHeader);

        const char vIn[] = 
        "in vec3 vertexPosition;            \n"
        "in vec3 vertexNormal;              \n"
        "in vec2 vertexTexCoord;            \n"
        "in vec4 vertexColor;             \n\n";
        fprintf(vertexFile, vIn);

        const char vOut[] = 
        "out vec3 fragPosition;             \n"
        "out vec3 fragNormal;               \n"
        "out vec2 fragTexCoord;             \n"
        "out vec4 fragColor;              \n\n";
        fprintf(vertexFile, vOut);

        const char vUniforms[] = 
        "uniform mat4 mvpMatrix;          \n\n";
        fprintf(vertexFile, vUniforms);
        
        fprintf(vertexFile, "// Constant values\n");
        int index = GetNodeIndex(nodes[0]->inputs[0]);
        CheckConstant(nodes[index], vertexFile);

        const char vMain[] = 
        "\nvoid main()                      \n"
        "{                                  \n"
        "    fragPosition = vertexPosition; \n"
        "    fragNormal = vertexNormal;     \n"
        "    fragTexCoord = vertexTexCoord; \n"
        "    fragColor = vertexColor;     \n\n";
        fprintf(vertexFile, vMain);

        CompileNode(nodes[index], vertexFile, false);

        switch (nodes[index]->output.dataCount)
        {
            case 1: fprintf(vertexFile, "\n    gl_Position = vec4(node_%02i, node_%02i, node_%02i, 1.0);\n}", nodes[0]->inputs[0], nodes[0]->inputs[0], nodes[0]->inputs[0]); break;
            case 2: fprintf(vertexFile, "\n    gl_Position = vec4(node_%02i.xy, 0.0, 1.0);\n}", nodes[0]->inputs[0]); break;
            case 3: fprintf(vertexFile, "\n    gl_Position = vec4(node_%02i.xyz, 1.0);\n}", nodes[0]->inputs[0]); break;
            case 4: fprintf(vertexFile, "\n    gl_Position = node_%02i;\n}", nodes[0]->inputs[0]); break;
            case 16: fprintf(vertexFile, "\n    gl_Position = node_%02i;\n}", nodes[0]->inputs[0]); break;
            default: break;
        }

        fclose(vertexFile);
    }
    else TraceLogFNode(true, "error when trying to open and write in vertex shader file");

    // Open fragment shader file to write data
    FILE *fragmentFile = fopen(FRAGMENT_PATH, "w");
    if (fragmentFile != NULL)
    {
        // Fragment shader definition to embed, no external file required
        const char fHeader[] = 
        "#version 330                     \n\n";
        fprintf(fragmentFile, fHeader);

        fprintf(fragmentFile, "// Input attributes\n");
        const char fIn[] = 
        "in vec3 fragPosition;             \n"
        "in vec3 fragNormal;               \n"
        "in vec2 fragTexCoord;             \n"
        "in vec4 fragColor;              \n\n";
        fprintf(fragmentFile, fIn);

        fprintf(fragmentFile, "// Uniform attributes\n");
        const char fUniforms[] = 
        "uniform sampler2D texture0;       \n"
        "uniform vec4 colDiffuse;          \n"
        "uniform vec3 viewDirection;       \n"
        "uniform mat4 modelMatrix;       \n\n";
        fprintf(fragmentFile, fUniforms);

        fprintf(fragmentFile, "// Output attributes\n");
        const char fOut[] = 
        "out vec4 finalColor;            \n\n";
        fprintf(fragmentFile, fOut);

        fprintf(fragmentFile, "// Constant values\n");
        int index = GetNodeIndex(nodes[1]->inputs[0]);
        CheckConstant(nodes[index], fragmentFile);

        const char fMain[] = 
        "\nvoid main()                      \n"
        "{                                  \n";
        fprintf(fragmentFile, fMain);

        CompileNode(nodes[index], fragmentFile, true);

        switch (nodes[index]->output.dataCount)
        {
            case 1: fprintf(fragmentFile, "\n    finalColor = vec4(node_%02i, node_%02i, node_%02i, 1.0);\n}", nodes[1]->inputs[0], nodes[1]->inputs[0], nodes[1]->inputs[0]); break;
            case 2: fprintf(fragmentFile, "\n    finalColor = vec4(node_%02i.xy, 0.0, 1.0);\n}", nodes[1]->inputs[0]); break;
            case 3: fprintf(fragmentFile, "\n    finalColor = vec4(node_%02i.xyz, 1.0);\n}", nodes[1]->inputs[0]); break;
            case 4: fprintf(fragmentFile, "\n    finalColor = node_%02i;\n}", nodes[1]->inputs[0]); break;
            default: break;
        }

        fclose(fragmentFile);
    }
    else TraceLogFNode(true, "error when trying to open and write in vertex shader file");

    shader = LoadShader(VERTEX_PATH, FRAGMENT_PATH);
    if (shader.id != 0)
    {
        loadedShader = true;
        model.material.shader = shader;
        viewUniform = GetShaderLocation(shader, "viewDirection");
        transformUniform = GetShaderLocation(shader, "modelMatrix");
    }
}

// Check nodes searching for constant values to define them in shaders
void CheckConstant(FNode node, FILE *file)
{
    switch (node->type)
    {
        case FNODE_PI: fprintf(file, "const float node_%02i = 3.14159265358979323846;\n"); break;
        case FNODE_E: fprintf(file, "const float node_%02i = 2.71828182845904523536;\n"); break;
        case FNODE_VALUE:
        {
            const char fConstantValue[] = "const float node_%02i = %.3f;\n";
            fprintf(file, fConstantValue, node->id, node->output.data[0].value);
        } break;
        case FNODE_VECTOR2:
        {
            const char fConstantVector2[] = "const vec2 node_%02i = vec2(%.3f, %.3f);\n";
            fprintf(file, fConstantVector2, node->id, node->output.data[0].value, node->output.data[1].value);
        } break;
        case FNODE_VECTOR3:
        {
            const char fConstantVector3[] = "const vec3 node_%02i = vec3(%.3f, %.3f, %.3f);\n";
            fprintf(file, fConstantVector3, node->id, node->output.data[0].value, node->output.data[1].value, node->output.data[2].value);
        } break;
        case FNODE_VECTOR4:
        {
            const char fConstantVector4[] = "const vec4 node_%02i = vec4(%.3f, %.3f, %.3f, %.3f);\n";
            fprintf(file, fConstantVector4, node->id, node->output.data[0].value, node->output.data[1].value, node->output.data[2].value, node->output.data[3].value);
        } break;
        default:
        {
            for (int i = 0; i < node->inputsCount; i++)
            {
                int index = GetNodeIndex(node->inputs[i]);
                CheckConstant(nodes[index], file);
            }
        } break;
    }
}

// Compiles a specific node checking its inputs and writing current node operation in shader
void CompileNode(FNode node, FILE *file, bool fragment)
{
    // Check if current node is an operator
    if ((node->inputsCount > 0) || ((node->type < FNODE_MATRIX) && (node->type > FNODE_E)))
    {
        // Check for operator nodes in inputs to compile them first
        for (int i = 0; i < node->inputsCount; i++)
        {
            int index = GetNodeIndex(node->inputs[i]);
            if (nodes[index]->type > FNODE_VECTOR4 || ((nodes[index]->type < FNODE_MATRIX) && (nodes[index]->type > FNODE_E))) CompileNode(nodes[index], file, fragment);
        }

        // Store variable declaration into a string
        char check[16] = { '\0' };
        switch (node->output.dataCount)
        {
            case 1: sprintf(check, "float node_%02i", node->id); break;
            case 2: sprintf(check, "vec2 node_%02i", node->id); break;
            case 3:
            {
                if (fragment) sprintf(check, "vec3 node_%02i", node->id);
                else if (node->type == FNODE_VERTEXPOSITION) sprintf(check, "vec4 node_%02i", node->id); 
            } break;
            case 4: sprintf(check, "vec4 node_%02i", node->id); break;
            case 16: sprintf(check, "mat4 node_%02i", node->id); break;
            default: break;
        }

        // Check if current node is previously defined and declared
        if (!FSearch(FRAGMENT_PATH, check))
        {
            // Variable definition based on current node output data count
            char body[1024] = { '\0' };
            char definition[32] = { '\0' };
            switch (node->output.dataCount)
            {
                case 1: sprintf(definition, "    float node_%02i = ", node->id); break;
                case 2: sprintf(definition, "    vec2 node_%02i = ", node->id); break;
                case 3:
                {
                    if (fragment) sprintf(definition, "    vec3 node_%02i = ", node->id);
                    else if (node->type == FNODE_VERTEXPOSITION) sprintf(definition, "    vec4 node_%02i = ", node->id);
                } break;
                case 4: sprintf(definition, "    vec4 node_%02i = ", node->id); break;
                case 16: sprintf(definition, "    mat4 node_%02i = ", node->id); break;
                default: break;
            }
            strcat(body, definition);

            if ((node->type < FNODE_MATRIX) && (node->type > FNODE_E))
            {
                char temp[32] = { '\0' };
                switch (node->type)
                {
                    case FNODE_VERTEXPOSITION:
                    {
                        if (fragment) strcat(body, "fragPosition;\n");
                        else strcat(body, "vec4(vertexPosition, 1.0);\n");
                    } break;
                    case FNODE_VERTEXNORMAL: strcat(body, "fragNormal;\n"); break;
                    case FNODE_FRESNEL: strcat(body, "1 - dot(fragNormal, viewDirection);\n"); break;
                    case FNODE_VIEWDIRECTION: strcat(body, "viewDirection;\n"); break;
                    case FNODE_MVP: strcat(body, "mvpMatrix;\n"); break;
                    default: break;
                }
            }
            else if ((node->type >= FNODE_ADD && node->type <= FNODE_DIVIDE) || node->type == FNODE_MULTIPLYMATRIX)
            {
                // Operate with each input node
                for (int i = 0; i < node->inputsCount; i++)
                {
                    char temp[32] = { '\0' };
                    if ((i+1) == node->inputsCount) sprintf(temp, "node_%02i;\n", node->inputs[i]);
                    else
                    {
                        sprintf(temp, "node_%02i", node->inputs[i]);
                        switch (node->type)
                        {
                            case FNODE_ADD: strcat(temp, " + "); break;
                            case FNODE_SUBTRACT: strcat(temp, " - "); break;
                            case FNODE_MULTIPLYMATRIX:
                            case FNODE_MULTIPLY: strcat(temp, "*"); break;
                            case FNODE_DIVIDE: strcat(temp, "/"); break;
                            default: break;
                        }
                    }

                    strcat(body, temp);
                }
            }
            else if (node->type >= FNODE_APPEND)
            {
                char temp[32] = { '\0' };
                switch (node->type)
                {
                    case FNODE_APPEND:
                    {
                        switch (node->output.dataCount)
                        {
                            case 1: sprintf(temp, "node_%02i;\n", node->inputs[0]); break;
                            case 2: sprintf(temp, "vec2(node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1]); break;
                            case 3: sprintf(temp, "vec3(node_%02i, node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1], node->inputs[2]); break;
                            case 4: sprintf(temp, "vec4(node_%02i, node_%02i, node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1], node->inputs[2], node->inputs[3]); break;
                            default: break;
                        }
                    } break;
                    case FNODE_ONEMINUS: sprintf(temp, "(1 - node_%02i);\n", node->inputs[0]); break;
                    case FNODE_ABS: sprintf(temp, "abs(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_COS: sprintf(temp, "cos(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_SIN: sprintf(temp, "sin(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_TAN: sprintf(temp, "tan(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_DEG2RAD: sprintf(temp, "node_%02i*(3.14159265358979323846/180.0);\n", node->inputs[0]); break;
                    case FNODE_RAD2DEG: sprintf(temp, "node_%02i*(180.0/3.14159265358979323846);\n", node->inputs[0]); break;
                    case FNODE_NORMALIZE: sprintf(temp, "normalize(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_NEGATE: sprintf(temp, "node_%02i*-1;\n", node->inputs[0]); break;
                    case FNODE_RECIPROCAL: sprintf(temp, "1/node_%02i;\n", node->inputs[0]); break;
                    case FNODE_SQRT: sprintf(temp, "sqrt(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_TRUNC: sprintf(temp, "trunc(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_ROUND: sprintf(temp, "round(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_CEIL: sprintf(temp, "ceil(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_CLAMP01: sprintf(temp, "clamp(node_%02i, 0.0, 1.0);\n", node->inputs[0]); break;
                    case FNODE_EXP2: sprintf(temp, "exp2(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_POWER: sprintf(temp, "pow(node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1]); break;
                    case FNODE_STEP: sprintf(temp, "((node_%02i <= node_%02i) ? 1.0 : 0.0);\n", node->inputs[0], node->inputs[1]); break;
                    case FNODE_POSTERIZE: sprintf(temp, "floor(node_%02i*node_%02i)/node_%02i;\n", node->inputs[0], node->inputs[1], node->inputs[1]); break;
                    case FNODE_MAX: sprintf(temp, "max(node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1], node->inputs[1]); break;
                    case FNODE_MIN: sprintf(temp, "min(node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1], node->inputs[1]); break;
                    case FNODE_LERP: sprintf(temp, "lerp(node_%02i, node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1], node->inputs[2]); break;
                    case FNODE_SMOOTHSTEP: sprintf(temp, "smoothstep(node_%02i, node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1], node->inputs[2]); break;
                    case FNODE_CROSSPRODUCT: sprintf(temp, "cross(node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1]); break;
                    case FNODE_DESATURATE:
                    {
                        switch (node->output.dataCount)
                        {
                            case 1: sprintf(temp, "mix(node_%02i, 0.3, node_%02i);\n", node->inputs[0], node->inputs[1]); break;
                            case 2: sprintf(temp, "vec2(mix(node_%02i.x, vec2(0.3, 0.59), node_%02i));\n", node->inputs[0], node->inputs[1]); break;
                            case 3: sprintf(temp, "vec3(mix(node_%02i.xyz, vec3(0.3, 0.59, 0.11), node_%02i));\n", node->inputs[0], node->inputs[1]); break;
                            case 4: sprintf(temp, "vec4(mix(node_%02i.xyz, vec3(0.3, 0.59, 0.11), node_%02i), 1.0);\n", node->inputs[0], node->inputs[1]); break;
                            default: break;
                        }
                    } break;
                    case FNODE_DISTANCE: sprintf(temp, "distance(node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1]); break;
                    case FNODE_DOTPRODUCT: sprintf(temp, "dot(node_%02i, node_%02i);\n", node->inputs[0], node->inputs[1]); break;
                    case FNODE_LENGTH: sprintf(temp, "length(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_TRANSPOSE: sprintf(temp, "transpose(node_%02i);\n", node->inputs[0]); break;
                    case FNODE_PROJECTION:
                    {
                        switch (node->output.dataCount)
                        {
                            case 2: sprintf(temp, "vec2(dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.x, dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.y);\n", 
                            node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1]); break;
                            case 3: sprintf(temp, "vec3(dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.x, dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.y, dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.z);\n", 
                            node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1]); break;
                            case 4: sprintf(temp, "vec4(dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.x, dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.y, dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.z, dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.w);\n", 
                            node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1]); break;
                            default: break;
                        }
                    }
                    case FNODE_REJECTION:
                    {
                        switch (node->output.dataCount)
                        {
                            case 2: sprintf(temp, "vec2(node_%02i.x - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.x, node_%02i.y - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.y);\n", 
                            node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1]); break;
                            case 3: sprintf(temp, "vec3(node_%02i.x - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.x, node_%02i.y - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.y, node_%02i.z - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.z);\n", 
                            node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1]); break;
                            case 4: sprintf(temp, "vec4(node_%02i.x - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.x, node_%02i.y - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.y, node_%02i.z - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.z, node_%02i.w - dot(node_%02i, node_%02i)/dot(node_%02i, node_%02i)*node_%02i.w);\n", 
                            node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[0], node->inputs[0], node->inputs[1], node->inputs[1], node->inputs[1], node->inputs[1]); break;
                            default: break;
                        }
                    } break;
                    case FNODE_HALFDIRECTION: sprintf(temp, "normalize(node_%02i + node_%02i);\n", node->inputs[0], node->inputs[1]); break;
                    default: break;
                }

                strcat(body, temp);
            }

            // Write current node string to shader file
            fprintf(file, body);
        }
    }
}

// Aligns all created nodes
void AlignAllNodes()
{
    for (int i = 0; i < nodesCount; i++)
    {
        AlignNode(nodes[i]);
        UpdateNodeShapes(nodes[i]);
    }
}

// Destroys all unused nodes
void ClearUnusedNodes()
{
    for (int i = nodesCount - 1; i >= 0; i--)
    {
        bool used = (nodes[i]->type >= FNODE_VERTEX);

        if (!used)
        {
            for (int k = 0; k < linesCount; k++)
            {
                if ((nodes[i]->id == lines[k]->from) || (nodes[i]->id == lines[k]->to))
                {
                    used = true;
                    break;
                }
            }
        }

        if (!used) DestroyNode(nodes[i]);
    }

    TraceLogFNode(false, "all unused nodes have been deleted [USED RAM: %i bytes]", usedMemory);
}

// Destroys all created nodes and its linked lines
void ClearGraph()
{
    for (int i = nodesCount - 1; i >= 0; i--)
    {
        if (nodes[i]->type < FNODE_VERTEX) DestroyNode(nodes[i]);
    }
    for (int i = commentsCount - 1; i >= 0; i--) DestroyComment(comments[i]);

    TraceLogFNode(false, "all nodes have been deleted [USED RAM: %i bytes]", usedMemory);
}

// Draw canvas space to create nodes
void DrawCanvas()
{    
    // Draw background title and credits
    DrawText("FNODE 1.0", (canvasSize.x - MeasureText("FNODE 1.0", 120))/2, canvasSize.y/2 - 60, 120, Fade(LIGHTGRAY, UI_GRID_ALPHA*2));
    DrawText("VICTOR FISAC", (canvasSize.x - MeasureText("VICTOR FISAC", 40))/2, canvasSize.y*0.65f - 20, 40, Fade(LIGHTGRAY, UI_GRID_ALPHA*2));

    Begin2dMode(camera);

        DrawCanvasGrid(UI_GRID_COUNT);

        // Draw all created comments, lines and nodes
        for (int i = 0; i < commentsCount; i++) DrawComment(comments[i]);
        for (int i = 0; i < nodesCount; i++) DrawNode(nodes[i]);
        for (int i = 0; i < linesCount; i++) DrawNodeLine(lines[i]);

    End2dMode();
}

// Draw canvas grid with a specific number of divisions for horizontal and vertical lines
void DrawCanvasGrid(int divisions)
{
    int spacing = 0;
    for (int i = 0; i < divisions; i++)
    {
        for (int k = 0; k < 5; k++)
        {
            DrawRectangle(-(divisions/2*UI_GRID_SPACING*5) + spacing, -100000, 1, 200000, ((k == 0) ? Fade(BLACK, UI_GRID_ALPHA*2) : Fade(GRAY, UI_GRID_ALPHA)));
            spacing += UI_GRID_SPACING;
        }
    }

    spacing = 0;
    for (int i = 0; i < divisions; i++)
    {
        for (int k = 0; k < 5; k++)
        {
            DrawRectangle(-100000, -(divisions/2*UI_GRID_SPACING*5) + spacing, 200000, 1, ((k == 0) ? Fade(BLACK, UI_GRID_ALPHA*2) : Fade(GRAY, UI_GRID_ALPHA)));
            spacing += UI_GRID_SPACING;
        }
    }
}

// Draws a visor with default model rotating and current shader
void DrawVisor()
{
    BeginTextureMode(visorTarget);
    
        DrawRectangle(0, 0, screenSize.x, screenSize.y, GRAY);

        Begin3dMode(camera3d);

            DrawModelEx(model, (Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 0, 1, 0 }, modelRotation, (Vector3){ 0.13f, 0.13f, 0.13f }, WHITE);

        End3dMode();

    EndTextureMode();

    Rectangle visor = { canvasSize.x - visorTarget.texture.width - UI_PADDING, screenSize.y - visorTarget.texture.height - UI_PADDING, visorTarget.texture.width, visorTarget.texture.height };
    DrawRectangle(visor.x - VISOR_BORDER, visor.y - VISOR_BORDER, visor.width + VISOR_BORDER*2, visor.height + VISOR_BORDER*2, BLACK);

    BeginShaderMode(fxaa);

        DrawTexturePro(visorTarget.texture, (Rectangle){ 0, 0, visorTarget.texture.width, -visorTarget.texture.height }, visor, (Vector2){ 0, 0 }, 0.0f, WHITE);

    EndShaderMode();
}

// Draw interface to create nodes
void DrawInterface()
{
    // Draw interface background
    DrawRectangleRec((Rectangle){ canvasSize.x, 0.0f, screenSize.x - canvasSize.x, screenSize.y }, DARKGRAY);

    // Draw interface main buttons
    if (FButton((Rectangle){ UI_PADDING, screenSize.y - (UI_BUTTON_HEIGHT + UI_PADDING), (screenSize.x - canvasSize.x - UI_PADDING*2)/2, UI_BUTTON_HEIGHT }, "Compile")) CompileShader(); menuOffset = 1;
    if (FButton((Rectangle){ UI_PADDING + ((screenSize.x - canvasSize.x - UI_PADDING*2)/2 + UI_PADDING)*menuOffset, screenSize.y - (UI_BUTTON_HEIGHT + UI_PADDING), (screenSize.x - canvasSize.x - UI_PADDING*2)/2, UI_BUTTON_HEIGHT }, "Clear Graph")) ClearGraph();
    if (FButton((Rectangle){ UI_PADDING + ((screenSize.x - canvasSize.x - UI_PADDING*2)/2 + UI_PADDING)*menuOffset, screenSize.y - (UI_BUTTON_HEIGHT + UI_PADDING), (screenSize.x - canvasSize.x - UI_PADDING*2)/2, UI_BUTTON_HEIGHT }, "Align Nodes")) AlignAllNodes();
    if (FButton((Rectangle){ UI_PADDING + ((screenSize.x - canvasSize.x - UI_PADDING*2)/2 + UI_PADDING)*menuOffset, screenSize.y - (UI_BUTTON_HEIGHT + UI_PADDING), (screenSize.x - canvasSize.x - UI_PADDING*2)/2, UI_BUTTON_HEIGHT }, "Clear Unused Nodes")) ClearUnusedNodes();

    // Draw interface nodes buttons
    DrawText("Constant Vectors", canvasSize.x + ((screenSize.x - canvasSize.x) - MeasureText("Constant Vectors", 10))/2 - UI_PADDING_SCROLL/2, UI_PADDING*4 - menuScroll, 10, WHITE); menuOffset = 1;
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Value")) CreateNodeValue((float)GetRandomValue(-11, 10));
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Vector 2")) CreateNodeVector2((Vector2){ (float)GetRandomValue(0, 10), (float)GetRandomValue(0, 10) });
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Vector 3")) CreateNodeVector3((Vector3){ (float)GetRandomValue(0, 10), (float)GetRandomValue(0, 10), (float)GetRandomValue(0, 10) });
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Vector 4")) CreateNodeVector4((Vector4){ (float)GetRandomValue(0, 10), (float)GetRandomValue(0, 10), (float)GetRandomValue(0, 10), (float)GetRandomValue(0, 10) });
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Matrix 4x4")) CreateNodeMatrix(FMatrixIdentity());

    DrawText("Arithmetic", canvasSize.x + ((screenSize.x - canvasSize.x) - MeasureText("Arithmetic", 10))/2 - UI_PADDING_SCROLL/2, UI_PADDING*4 + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, 10, WHITE); menuOffset++;
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Add")) CreateNodeOperator(FNODE_ADD, "Add", MAX_INPUTS);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Subtract")) CreateNodeOperator(FNODE_SUBTRACT, "Subtract", MAX_INPUTS);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Multiply")) CreateNodeOperator(FNODE_MULTIPLY, "Multiply", MAX_INPUTS);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Multiply Matrix")) CreateNodeOperator(FNODE_MULTIPLYMATRIX, "Multiply Matrix", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Divide")) CreateNodeOperator(FNODE_DIVIDE, "Divide", MAX_INPUTS);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "One Minus")) CreateNodeOperator(FNODE_ONEMINUS, "One Minus", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Abs")) CreateNodeOperator(FNODE_ABS, "Abs", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Clamp 0-1")) CreateNodeOperator(FNODE_CLAMP01, "Clamp 0-1", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Max")) CreateNodeOperator(FNODE_MAX, "Max", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Min")) CreateNodeOperator(FNODE_MIN, "Min", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Negate")) CreateNodeOperator(FNODE_NEGATE, "Negate", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Reciprocal")) CreateNodeOperator(FNODE_RECIPROCAL, "Reciprocal", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Square Root")) CreateNodeOperator(FNODE_SQRT, "Square Root", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Power")) CreateNodeOperator(FNODE_POWER, "Power", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Exp 2")) CreateNodeOperator(FNODE_EXP2, "Exp 2", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Posterize")) CreateNodeOperator(FNODE_POSTERIZE, "Posterize", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Ceil")) CreateNodeOperator(FNODE_CEIL, "Ceil", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Round")) CreateNodeOperator(FNODE_ROUND, "Round", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Trunc")) CreateNodeOperator(FNODE_TRUNC, "Trunc", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Lerp")) CreateNodeOperator(FNODE_LERP, "Lerp", 3);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Step")) CreateNodeOperator(FNODE_STEP, "Step", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "SmoothStep")) CreateNodeOperator(FNODE_SMOOTHSTEP, "SmoothStep", 3);

    DrawText("Vector Operations", canvasSize.x + ((screenSize.x - canvasSize.x) - MeasureText("Vector Operations", 10))/2 - UI_PADDING_SCROLL/2, UI_PADDING*4 + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, 10, WHITE); menuOffset++;
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Append")) CreateNodeOperator(FNODE_APPEND, "Append", 4);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Normalize")) CreateNodeOperator(FNODE_NORMALIZE, "Normalize", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Cross Product")) CreateNodeOperator(FNODE_CROSSPRODUCT, "Cross Product", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Desaturate")) CreateNodeOperator(FNODE_DESATURATE, "Desaturate", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Distance")) CreateNodeOperator(FNODE_DISTANCE, "Distance", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Dot Product")) CreateNodeOperator(FNODE_DOTPRODUCT, "Dot Product", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Length")) CreateNodeOperator(FNODE_LENGTH, "Length", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Transpose")) CreateNodeOperator(FNODE_TRANSPOSE, "Transpose", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Vector Projection")) CreateNodeOperator(FNODE_PROJECTION, "Vector Projection", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Vector Rejection")) CreateNodeOperator(FNODE_REJECTION, "Vector Rejection", 2);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Half Direction")) CreateNodeOperator(FNODE_HALFDIRECTION, "Half Direction", 2);

    DrawText("Geometry Data", canvasSize.x + ((screenSize.x - canvasSize.x) - MeasureText("Geometry Data", 10))/2 - UI_PADDING_SCROLL/2, UI_PADDING*4 + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, 10, WHITE); menuOffset++;
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Vertex Position")) CreateNodeUniform(FNODE_VERTEXPOSITION, "Vertex Position", 3);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Normal Direction")) CreateNodeUniform(FNODE_VERTEXNORMAL, "Normal Direction", 3);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "View Direction")) CreateNodeUniform(FNODE_VIEWDIRECTION, "View Direction", 3);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Fresnel")) CreateNodeUniform(FNODE_FRESNEL, "Fresnel", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "MVP Matrix")) CreateNodeUniform(FNODE_MVP, "MVP Matrix", 16);

    DrawText("Math Constants", canvasSize.x + ((screenSize.x - canvasSize.x) - MeasureText("Math Constants", 10))/2 - UI_PADDING_SCROLL/2, UI_PADDING*4 + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, 10, WHITE); menuOffset++;
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "PI")) CreateNodePI();
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "e")) CreateNodeE();

    DrawText("Trigonometry", canvasSize.x + ((screenSize.x - canvasSize.x) - MeasureText("Trigonometry", 10))/2 - UI_PADDING_SCROLL/2, UI_PADDING*4 + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, 10, WHITE); menuOffset++;
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Cosine")) CreateNodeOperator(FNODE_COS, "Cosine", 1);
    if (FButton((Rçectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Sine")) CreateNodeOperator(FNODE_SIN, "Sine", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Tangent")) CreateNodeOperator(FNODE_TAN, "Tangent", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Deg to Rad")) CreateNodeOperator(FNODE_DEG2RAD, "Deg to Rad", 1);
    if (FButton((Rectangle){ canvasSize.x + UI_PADDING, UI_PADDING + (UI_BUTTON_HEIGHT + UI_PADDING)*menuOffset - menuScroll, screenSize.x - canvasSize.x - UI_PADDING*2 - UI_PADDING_SCROLL, UI_BUTTON_HEIGHT }, "Rad to Deg")) CreateNodeOperator(FNODE_RAD2DEG, "Rad to Deg", 1);

    DrawRectangle(menuScrollRec.x - 3, 2, menuScrollRec.width + 6, screenSize.y - 4, (Color){ UI_BORDER_DEFAULT_COLOR, UI_BORDER_DEFAULT_COLOR, UI_BORDER_DEFAULT_COLOR, 255 });
    DrawRectangle(menuScrollRec.x - 2, menuScrollRec.y - 2, menuScrollRec.width + 4, menuScrollRec.height + 4, DARKGRAY);
    DrawRectangleRec(menuScrollRec, ((scrollState == 1) ? LIGHTGRAY : RAYWHITE));

    if (debugMode)
    {
        const char *string = 
        "loadedShader: %i\n"
        "selectedNode: %i\n"
        "editNode: %i\n"
        "lineState: %i\n"
        "commentState: %i\n"
        "selectedComment: %i\n"
        "editSize: %i\n"
        "editSizeType: %i\n"
        "editComment: %i\n"
        "editNodeText: %s";

        DrawText(FormatText(string, loadedShader, selectedNode, editNode, lineState, commentState, selectedComment, editSize, editSizeType, editComment, ((editNodeText != NULL) ? editNodeText : "NULL")), 10, 30, 10, BLACK);

        DrawFPS(10, 10);
    }
}