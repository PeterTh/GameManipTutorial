# Preamble

## What is this about?
This document is a tutorial for manipulating the rendering of a game (generally to increase its quality) if you only have a binary available. If yo uever wondered how something like DSFix, or DPFix, or many of my GeDoSaTo plugins work, then this is for you.

If you have ever thought it would be great if you could do something like that too, then even better, this is for you too. Hopefully it will save you a lot of time figuring out things that become second nature when you have been doing this for half a decade or so.

## Structure
There are 3 major sections in this document: "Preamble" (what you are reading right now), "Analysis", and "Manipulation". "Analysis" deals with figuring out what the game does and what we probably need to change, while "Manipulation" explains how to apply the actual changes. 

## Prerequisites
In this tutorial, we will be using this software:
* The *excellent* [RenderDoc](https://renderdoc.org/), which is a free graphics debugging tool that makes all of this **so much** easier than back when I was messign around with textual log files and image dumps.
* Visual Studio 2017 Community Edition, which is freely available from Microsoft.

In terms of foundational knowledge, to get the full benefit of this tutorial, it would probably be good to have:
* Basic knowledge of C++.
* An understanding of the fundamentals of 3D rendering.
* Some knowledge about Screen-Space Ambient Occlusion. 

## The game of coice
We will be dealing with Nier:Automata, because it is a great game and because it offers what I'd consider a "moderate" amount of challenge for the types of tasks we wish to perform. It also plays well with Renderdoc without any complicated coaxing. Of course, the tutorial should be equally applicable to a great many other games.

# Analysis

## Our goal
For the purpose of a tutorial (and for any type of work and modding really) it's important to have a very clear goal. Our **goal** for this exercise is to *increase the spatial rendering resolition* of the Ambient Occlusion effect in Nier:Automata. I arrived at this goal (like most my modding goals) by playing the game and seeing something I didn't like, image quality wise.

## A Reference Run of Renderdoc
To gain an understanding of how the game performs its rendering, we will run it from within Renderdoc and *capture a frame*.
This will allow us to investigate everything that happens which is related to rendering that particular frame. To do so, we need to point Renderdocat the game executable and launch it:
![Image](img/01_renderdoc_capture.png?raw=true)
Then, in-game, we move to a location which should be a good place to judge the effect we want to manipulate, and capture a frame by pressing F12 (Renderdoc should show an in-game overlay which informs us of this shortcut). After exiting the game, Renderdoc will automatically load and show us the framedump in replay mode:
![Image](img/02_renderdoc_ao_pass.png?raw=true)
I won't give a full explanation of all the UI elements of Renderdoc. What is important for us now is that on the left, the *Event Browser* gives a chronological timeline of all rendering events, and that the *Texture Viewer* allows us to see the output of each rendering call. By opening the texture viewer and navigting downwards through the events, we should eventually reach something which looks like a raw ambient occlusion buffer. It is shown in the screenshot above. What we immediately see there is that it is a **800x450** buffer, despite our rendering resolution being a full 2560x1440. This is likely the primary culprit for the low quality we are seeing, and it is what we need to change.

## Gaining a better understanding

However, in order to understand everything we need to change, we also need to know which inputs this AO pass uses. Luckily, Renderdoc makes this rather easy:
![Image](img/03_renderdoc_ao_inputs.png?raw=true)
We've switched to the *Inputs* tab on the right, and now see that we only have a single input. Just like our output, it's 800 by 450. Note that initially it would just be shown as a flat black area. This is often the case with floating point buffers, if their values don't fall into the ranges we can distinguish with the naked eye. As you can see in the highlighted part on top, we can manipulate the *Range* setting in Renderdoc to make the content visible. In this case, it clearly looks like a Z buffer (storing a depth value for each pixel), which is what we would expect as the minimum input to an SSAO pass.

Interestingly, we also see (at the bottom) that it is a texture with 10 mipmaps. Further investigation of the rendering passes using it as either a source or target reveals that the individual mip maps are populated just before the AO pass. Here it helps a lot to have read the Scalable Ambient Obscurance paper, since it explains how a hierarchical Z-buffer can be used to greatly speed up and reduce the memory overhead of large-scale AO computations.

Looking at the rendering calls right after the initial AO pass, we see a horizontal and vertical depth-dependent blur pass, which is also typical of most SSAO implementations.

## Summary

To summarize, our initial analysis tells us that:
* There are two 800x450 buffers involved. One is of the `R8G8B8A8_UNORM` format and stores the AO coverage, the other is of the `R32_FLOAT` format with mipmaps and stores a hierarchical Z buffer.
* In terms of rendering, first mipmap 0 of the R32 buffer is populated, and then subsequently smaller mipmaps are filled from larger ones.
* Then the actual AO pass is calculated.
* Finally, horizontal and vertical depth-dependent blur passes are performed.

We will refer to this Renderdoc frame capture as our *initial reference*, which we can use to look up what should really be happening in case something goes wrong (and it always does) once we start manipulating things.
