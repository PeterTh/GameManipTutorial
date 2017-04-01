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
We will be dealing with Nier:Automata, because it is a great game and because it offers what I'd consider a "moderate" amount of challenge for the types of tasks we wish to perform. Of course, the tutorial should be equally applicable to a great many other games.

# Analysis

## Our goal
For the purpose of a tutorial (and for any type of work and modding really) it's important to have a very clear goal. Our **goal** for this exercise is to *increase the spatial rendering resolition* of the Ambient Occlusion effect in Nier:Automata. I arrived at this goal (like most my modding goals) by playing the game and seeing something I didn't like, image quality wise.
