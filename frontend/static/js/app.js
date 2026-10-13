// Small helpers used by the templates. No framework needed.

// ---- login / signup pop-ups ----
function openModal(id)  { document.getElementById(id).classList.add('open'); }
function closeModal(id) { document.getElementById(id).classList.remove('open'); }

document.addEventListener('click', function (e) {
  if (e.target.classList && e.target.classList.contains('modal')) e.target.classList.remove('open');
});

document.addEventListener('keydown', function (e) {
  if (e.key === 'Escape')
    document.querySelectorAll('.modal.open').forEach(function (m) { m.classList.remove('open'); });
});

// ---- quantity "-" / "+" buttons: <button onclick="stepQty(this,1)"> ----
function stepQty(button, delta) {
  var input = button.parentNode.querySelector('input');
  var value = parseInt(input.value || '1', 10) + delta;
  var min = parseInt(input.min || '1', 10);
  var max = parseInt(input.max || '99', 10);
  input.value = Math.max(min, Math.min(max, value));
}

// ---- hide the green/red message bar after a few seconds ----
window.addEventListener('load', function () {
  var flash = document.querySelector('.flash');
  if (flash) setTimeout(function () { flash.style.display = 'none'; }, 5000);
});
