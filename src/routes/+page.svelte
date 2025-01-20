<script>
  import { onMount } from"svelte";
  import Home from "$lib/Home.svelte";
  import Projects from "$lib/Projects.svelte";
  import WorkExperience from "$lib/WorkExperience.svelte";
  import Skills from "$lib/Skills.svelte"

  let components = [Home, WorkExperience, Skills, Projects];
  let currentIndex = $state(0);
  let scrollPosition = $state(0);
  let scrollDirection = $state(1);

  onMount(() => {
    window.addEventListener("wheel", (e) => {
      if (e.deltaY > 0) {
        scrollDirection = 1;
      } else {
        scrollDirection = -1;
      }
      scrollPosition += scrollDirection * 100;
      if (scrollPosition > 100) {
        scrollPosition = 0;
        currentIndex = (currentIndex + 1) % components.length;
      } else if (scrollPosition < -100) {
        scrollPosition = 0;
        currentIndex = (currentIndex - 1 + components.length) % components.length;
      }
    });
  });

  function getTransform(index) {
    const offset = (index - currentIndex) * 100;
    const opacity = 1 - Math.abs(offset) / 100;
    const scale = 1 - Math.abs(offset) / 200;
    const blur = Math.abs(offset) / 10;
    return { offset, scale, opacity, blur };
  }

  let transform = getTransform(currentIndex);
</script>

<div class="h-screen w-screen relative overflow-hidden">
  {#each components as Component, index}
  <div class="absolute top-0 left-0 w-full h-full transition-transform duration-500" style={`transform: translateX(${transform.offset}%) scale(${transform.scale}); opacity: ${transform.opacity}; filter: blur(${transform.blur}px)`}>
      <Component />
    </div>
  {/each}
</div>
